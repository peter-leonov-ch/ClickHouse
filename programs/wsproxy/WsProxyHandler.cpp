#include <WsProxyHandler.h>
#include <WebSocketFrames.h>

#include <Server/HTTP/HTTPServerRequest.h>
#include <Server/HTTP/HTTPServerResponse.h>

#include <Common/Exception.h>
#include <Common/logger_useful.h>

#include <IO/Operators.h>
#include <IO/WriteBuffer.h>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>

#include <base/scope_guard.h>

#include <algorithm>


namespace DB
{

using namespace DB::WsProxy;

namespace
{

/// Per RFC 7230 the `Connection` header is a comma-separated token list. Match
/// the exact `upgrade` token rather than a substring (which would also accept
/// unrelated values containing those letters).
bool hasUpgradeToken(const String & connection_header)
{
    String value = connection_header;
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    size_t pos = 0;
    while (pos <= value.size())
    {
        size_t comma = value.find(',', pos);
        size_t end_pos = (comma == String::npos) ? value.size() : comma;
        size_t start = value.find_first_not_of(" \t", pos);
        if (start != String::npos && start < end_pos)
        {
            size_t last = value.find_last_not_of(" \t", end_pos - 1);
            if (value.substr(start, last - start + 1) == "upgrade")
                return true;
        }
        if (comma == String::npos)
            break;
        pos = comma + 1;
    }
    return false;
}

}

void WsProxyHandler::serveInfo(HTTPServerRequest & request, HTTPServerResponse & response)
{
    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/plain; charset=UTF-8");
    *response.send()
        << "clickhouse-wsproxy\n"
        << "Requested: " << request.getURI() << "\n"
        << "Connect with a WebSocket client to open a session. Native-protocol\n"
        << "bridging to a backend server is not implemented yet (step 3); the\n"
        << "current build echoes WebSocket messages back.\n";
}

void WsProxyHandler::handleWebSocket(HTTPServerRequest & request, HTTPServerResponse & response)
{
    LoggerPtr log = getLogger("WsProxyHandler");

    if (request.getMethod() != HTTPRequest::HTTP_GET)
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_METHOD_NOT_ALLOWED);
        *response.send() << "WebSocket upgrade requires GET method.\n";
        return;
    }

    String ws_key = request.get("Sec-WebSocket-Key", "");
    if (!isValidWebSocketKey(ws_key))
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        *response.send() << "Invalid or missing Sec-WebSocket-Key.\n";
        return;
    }

    /// RFC 6455 fixed the protocol at version 13; earlier drafts are obsolete.
    if (request.get("Sec-WebSocket-Version", "") != "13")
    {
        response.set("Sec-WebSocket-Version", "13");
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        *response.send() << "Unsupported WebSocket version.\n";
        return;
    }

    /// Complete the handshake by writing 101 directly to the socket; from here
    /// on the stream is in WebSocket framing mode and we own the socket.
    ///
    /// Note: unlike the server's web terminal, no `Origin` check is enforced
    /// here. The proxy's clients are applications, not browsers, so browser
    /// cross-site protections do not apply; authentication will be handled in
    /// step 3 as part of opening the backend connection.
    Poco::Net::StreamSocket & socket = response.getSocket();
    const String handshake
        = "HTTP/1.1 101 Switching Protocols\r\n"
          "Upgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Accept: "
        + computeWebSocketAccept(ws_key) + "\r\n\r\n";

    /// `sendWebSocketFrame` and friends throw on write failure; reuse the frame
    /// writer's guarantee by sending the raw handshake through the socket.
    size_t sent_total = 0;
    while (sent_total < handshake.size())
    {
        int sent = socket.sendBytes(handshake.data() + sent_total, static_cast<int>(handshake.size() - sent_total));
        if (sent <= 0)
        {
            LOG_DEBUG(log, "Failed to write WebSocket handshake");
            return;
        }
        sent_total += static_cast<size_t>(sent);
    }

    SCOPE_EXIT({
        try
        {
            socket.shutdown();
        }
        catch (...)
        {
            tryLogCurrentException(log, "Failed to shut down WebSocket socket");
        }
    });

    LOG_DEBUG(log, "WebSocket session established (echo mode)");

    /// Bound each blocking read so a stalled client cannot pin the handler thread.
    socket.setReceiveTimeout(Poco::Timespan(30, 0));

    bool running = true;
    bool close_sent = false;
    auto send_close_once = [&](uint16_t code, const String & reason)
    {
        if (close_sent)
            return;
        close_sent = true;
        sendWebSocketClose(socket, code, reason);
    };

    /// Fragment reassembly state (RFC 6455 section 5.4).
    String message_buffer;
    uint8_t message_opcode = 0;
    bool in_fragmented_message = false;
    static constexpr size_t MAX_MESSAGE_SIZE = 16 * 1024 * 1024;

    while (running)
    {
        WebSocketFrame frame;
        try
        {
            frame = readWebSocketFrame(socket);
        }
        catch (const Poco::TimeoutException &)
        {
            LOG_DEBUG(log, "WebSocket read timed out, closing");
            break;
        }
        catch (...)
        {
            LOG_DEBUG(log, "WebSocket read error: {}", getCurrentExceptionMessage(false));
            break;
        }

        if (frame.message_too_big)
        {
            send_close_once(1009, "Message too big");
            break;
        }
        if (frame.protocol_error)
        {
            send_close_once(1002, "Protocol error");
            break;
        }
        if (!frame.valid)
            break;

        /// Control frames may interleave with fragmented data frames.
        if (frame.opcode >= 0x08)
        {
            switch (frame.opcode)
            {
                case Opcode::Close:
                    send_close_once(1000, "Bye");
                    running = false;
                    break;
                case Opcode::Ping:
                    sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
                    break;
                default:
                    break;
            }
            continue;
        }

        /// Data frame: accumulate fragments.
        if (frame.opcode != Opcode::Continuation)
        {
            if (in_fragmented_message)
            {
                send_close_once(1002, "New message during fragmentation");
                break;
            }
            message_opcode = frame.opcode;
            message_buffer = std::move(frame.payload);
            in_fragmented_message = !frame.fin;
        }
        else
        {
            if (!in_fragmented_message)
            {
                send_close_once(1002, "Unexpected continuation frame");
                break;
            }
            if (message_buffer.size() + frame.payload.size() > MAX_MESSAGE_SIZE)
            {
                send_close_once(1009, "Message too big");
                break;
            }
            message_buffer.append(frame.payload);
            if (frame.fin)
                in_fragmented_message = false;
        }

        if (!frame.fin)
            continue; /// More fragments to come.

        /// Complete message assembled: echo it back with the same opcode.
        sendWebSocketFrame(socket, message_opcode, message_buffer.data(), message_buffer.size());
        message_buffer.clear();
    }

    LOG_DEBUG(log, "WebSocket session closed");
}

void WsProxyHandler::handleRequest(HTTPServerRequest & request, HTTPServerResponse & response, const ProfileEvents::Event &)
{
    try
    {
        String upgrade = request.get("Upgrade", "");
        std::transform(upgrade.begin(), upgrade.end(), upgrade.begin(), ::tolower);

        if (upgrade == "websocket" && hasUpgradeToken(request.get("Connection", "")))
            handleWebSocket(request, response);
        else
            serveInfo(request, response);
    }
    catch (...)
    {
        tryLogCurrentException("WsProxyHandler");
    }
}

}
