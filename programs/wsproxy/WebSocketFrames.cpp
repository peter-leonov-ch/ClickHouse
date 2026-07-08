#include <WebSocketFrames.h>

#include <Common/Base64.h>

#include <Poco/SHA1Engine.h>

#include <ctime>


namespace DB::WsProxy
{

namespace
{

/// Monotonic clock in nanoseconds, self-contained so the framing layer has no
/// dependency beyond libc. Used only to enforce absolute read deadlines.
UInt64 monotonicNs()
{
    struct timespec ts
    {
    };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<UInt64>(ts.tv_sec) * 1'000'000'000ULL + static_cast<UInt64>(ts.tv_nsec);
}

/// Send all bytes to the socket, handling partial writes.
void sendAllBytes(Poco::Net::StreamSocket & socket, const char * data, size_t len)
{
    size_t total = 0;
    while (total < len)
    {
        int sent = socket.sendBytes(data + total, static_cast<int>(len - total));
        if (sent <= 0)
            throw Poco::IOException("Failed to send bytes to WebSocket");
        total += static_cast<size_t>(sent);
    }
}

/// Read exactly `n` bytes. If `deadline_ns` is non-zero, abort when the
/// monotonic clock passes it.
bool readExact(Poco::Net::StreamSocket & socket, char * buf, size_t n, UInt64 deadline_ns)
{
    size_t total = 0;
    while (total < n)
    {
        if (deadline_ns && monotonicNs() > deadline_ns)
            return false;
        int received = socket.receiveBytes(buf + total, static_cast<int>(n - total));
        if (received <= 0)
            return false;
        total += static_cast<size_t>(received);
    }
    return true;
}

}

String computeWebSocketAccept(const String & key)
{
    const String magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    Poco::SHA1Engine sha1;
    sha1.update(key + magic);
    auto digest = sha1.digest();
    return base64Encode(String(reinterpret_cast<const char *>(digest.data()), digest.size()));
}

bool isValidWebSocketKey(const String & key)
{
    if (key.empty() || key.size() > 128)
        return false;
    try
    {
        return base64Decode(key).size() == 16;
    }
    catch (...) /// Ok: malformed base64 is just an invalid key; the caller maps it to a 400.
    {
        return false;
    }
}

void sendWebSocketFrame(Poco::Net::StreamSocket & socket, uint8_t opcode, const char * data, size_t len)
{
    uint8_t header[10];
    size_t header_len = 2;

    header[0] = 0x80 | opcode; /// FIN + opcode

    if (len < 126)
    {
        header[1] = static_cast<uint8_t>(len);
    }
    else if (len < 65536)
    {
        header[1] = 126;
        header[2] = static_cast<uint8_t>((len >> 8) & 0xFF);
        header[3] = static_cast<uint8_t>(len & 0xFF);
        header_len = 4;
    }
    else
    {
        header[1] = 127;
        for (int i = 0; i < 8; ++i)
            header[2 + i] = static_cast<uint8_t>((len >> (56 - 8 * i)) & 0xFF);
        header_len = 10;
    }

    /// Combine header and payload into a single send to avoid partial frame writes.
    String buf;
    buf.reserve(header_len + len);
    buf.append(reinterpret_cast<const char *>(header), header_len);
    if (len > 0)
        buf.append(data, len);
    sendAllBytes(socket, buf.data(), buf.size());
}

void sendWebSocketBinary(Poco::Net::StreamSocket & socket, const char * data, size_t len)
{
    sendWebSocketFrame(socket, Opcode::Binary, data, len);
}

void sendWebSocketText(Poco::Net::StreamSocket & socket, const String & text)
{
    sendWebSocketFrame(socket, Opcode::Text, text.data(), text.size());
}

void sendWebSocketClose(Poco::Net::StreamSocket & socket, uint16_t code, const String & reason)
{
    String payload;
    payload.push_back(static_cast<char>((code >> 8) & 0xFF));
    payload.push_back(static_cast<char>(code & 0xFF));
    payload.append(reason);
    sendWebSocketFrame(socket, Opcode::Close, payload.data(), payload.size());
}

WebSocketFrame readWebSocketFrame(Poco::Net::StreamSocket & socket, UInt64 deadline_ns, uint64_t max_payload_size)
{
    WebSocketFrame frame;
    uint8_t header[2];

    if (!readExact(socket, reinterpret_cast<char *>(header), 2, deadline_ns))
        return frame;

    frame.fin = (header[0] & 0x80) != 0;
    frame.opcode = header[0] & 0x0F;

    /// RSV1/RSV2/RSV3 must be zero (no extensions negotiated).
    if (header[0] & 0x70)
    {
        frame.protocol_error = true;
        return frame;
    }

    /// Reject reserved opcodes per RFC 6455 section 5.2.
    if ((frame.opcode >= 0x03 && frame.opcode <= 0x07) || frame.opcode >= 0x0B)
    {
        frame.protocol_error = true;
        return frame;
    }

    bool masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;

    /// RFC 6455: client-to-server frames MUST be masked.
    if (!masked)
    {
        frame.protocol_error = true;
        return frame;
    }

    /// Control frames (opcode >= 0x08) must have FIN set and payload <= 125.
    if (frame.opcode >= 0x08)
    {
        if (!frame.fin || payload_len > 125)
        {
            frame.protocol_error = true;
            return frame;
        }
    }

    if (payload_len == 126)
    {
        uint8_t ext[2];
        if (!readExact(socket, reinterpret_cast<char *>(ext), 2, deadline_ns))
            return frame;
        payload_len = (static_cast<uint64_t>(ext[0]) << 8) | ext[1];
    }
    else if (payload_len == 127)
    {
        uint8_t ext[8];
        if (!readExact(socket, reinterpret_cast<char *>(ext), 8, deadline_ns))
            return frame;
        payload_len = 0;
        for (const auto & byte : ext)
            payload_len = (payload_len << 8) | byte;
    }

    /// Cap the payload before allocating, so a crafted length header cannot
    /// force a huge allocation. Distinct from `protocol_error` so the caller
    /// can close with the dedicated 1009 code.
    if (payload_len > max_payload_size)
    {
        frame.message_too_big = true;
        return frame;
    }

    uint8_t mask_key[4] = {};
    if (!readExact(socket, reinterpret_cast<char *>(mask_key), 4, deadline_ns))
        return frame;

    frame.payload.resize(payload_len);
    if (payload_len > 0)
    {
        if (!readExact(socket, frame.payload.data(), payload_len, deadline_ns))
            return frame;

        for (uint64_t i = 0; i < payload_len; ++i)
            frame.payload[i] ^= static_cast<char>(mask_key[i % 4]);
    }

    frame.valid = true;
    return frame;
}

}
