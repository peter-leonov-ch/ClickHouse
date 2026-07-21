#pragma once

#include <base/types.h>

#include <Poco/Net/StreamSocket.h>

/// Low-level RFC 6455 WebSocket framing over a raw stream socket.
///
/// Lifted and adapted from `src/Server/WebTerminalRequestHandler.cpp`, factored
/// into a reusable module because the proxy session loop (native protocol <->
/// WebSocket) will drive these primitives directly. Server-to-client frames are
/// never masked; client-to-server frames MUST be masked (enforced on read).

namespace DB::WsProxy
{

/// WebSocket opcodes (RFC 6455 section 5.2).
namespace Opcode
{
    static constexpr uint8_t Continuation = 0x0;
    static constexpr uint8_t Text = 0x1;
    static constexpr uint8_t Binary = 0x2;
    static constexpr uint8_t Close = 0x8;
    static constexpr uint8_t Ping = 0x9;
    static constexpr uint8_t Pong = 0xA;
}

struct WebSocketFrame
{
    uint8_t opcode = 0;
    bool fin = false;
    String payload;
    /// A fully and correctly read frame.
    bool valid = false;
    /// Set on an RFC 6455 protocol violation; caller closes with 1002.
    bool protocol_error = false;
    /// Set when the advertised payload exceeds `max_payload_size`; caller closes with 1009.
    bool message_too_big = false;
    /// Set when a close reason is not valid UTF-8; caller closes with 1007.
    bool invalid_utf8 = false;
};

/// Compute the `Sec-WebSocket-Accept` response value per RFC 6455 section 4.2.2.
String computeWebSocketAccept(const String & key);

/// Validate that `Sec-WebSocket-Key` is a base64-encoded 16-byte nonce.
bool isValidWebSocketKey(const String & key);

/// Send a single unmasked frame with the given opcode.
void sendWebSocketFrame(Poco::Net::StreamSocket & socket, uint8_t opcode, const char * data, size_t len);
void sendWebSocketBinary(Poco::Net::StreamSocket & socket, const char * data, size_t len);
void sendWebSocketText(Poco::Net::StreamSocket & socket, const String & text);
void sendWebSocketClose(Poco::Net::StreamSocket & socket, uint16_t code, const String & reason);

/// Read a single frame. `deadline_ns == 0` means no absolute deadline (per-read
/// timeouts are still governed by the socket's receive timeout). `max_payload_size`
/// caps the advertised payload length before any allocation. If
/// `completion_timeout_ns` is non-zero, it starts after the first byte arrives and
/// limits the remaining frame read without limiting idle time between frames.
WebSocketFrame readWebSocketFrame(
    Poco::Net::StreamSocket & socket,
    UInt64 deadline_ns = 0,
    uint64_t max_payload_size = 16 * 1024 * 1024,
    UInt64 completion_timeout_ns = 0);

}
