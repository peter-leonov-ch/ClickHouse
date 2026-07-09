#include <WsProxyHandler.h>
#include <WebSocketFrames.h>
#include <ProxySession.h>

#include <Server/HTTP/HTTPServerRequest.h>
#include <Server/HTTP/HTTPServerResponse.h>

#include <Common/Exception.h>
#include <Common/logger_useful.h>

#include <IO/Operators.h>
#include <IO/WriteBuffer.h>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/URI.h>

#include <base/scope_guard.h>

#include <algorithm>
#include <utility>


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

/// Read a query parameter from the WebSocket URL, returning `fallback` if absent.
String queryParam(const String & uri_string, const String & name, const String & fallback, LoggerPtr log)
{
    try
    {
        Poco::URI uri(uri_string);
        for (const auto & param : uri.getQueryParameters())
        {
            if (param.first == name && !param.second.empty())
                return param.second;
        }
    }
    catch (...)
    {
        LOG_DEBUG(log, "Could not parse request URI for the {} parameter; using default", name);
    }
    return fallback;
}

}

WsProxyHandler::WsProxyHandler(ContextPtr context_, BackendParams backend_)
    : context(std::move(context_)), backend(std::move(backend_))
{
}

void WsProxyHandler::serveInfo(HTTPServerRequest & request, HTTPServerResponse & response)
{
    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/plain; charset=UTF-8");
    *response.send()
        << "clickhouse-wsproxy\n"
        << "Requested: " << request.getURI() << "\n"
        << "Open a WebSocket connection to run queries. Send a query as a text\n"
        << "frame; results stream back as binary frames in the output format\n"
        << "(set via ?format=..., default JSONEachRow), followed by a JSON\n"
        << "control frame ({\"event\":\"end\"} / \"error\" / \"cancelled\").\n";
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
    /// cross-site protections do not apply; authentication is a later concern.
    Poco::Net::StreamSocket & socket = response.getSocket();
    const String handshake
        = "HTTP/1.1 101 Switching Protocols\r\n"
          "Upgrade: websocket\r\n"
          "Connection: Upgrade\r\n"
          "Sec-WebSocket-Accept: "
        + computeWebSocketAccept(ws_key) + "\r\n\r\n";

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
            /// The client may have already closed the socket; expected, not worth a stack trace.
            LOG_DEBUG(log, "WebSocket socket shutdown: {}", getCurrentExceptionMessage(false));
        }
    });

    const String & uri = request.getURI();
    const String out_format = queryParam(uri, "format", "JSONEachRow", log);
    /// Optional: `?logs=<level>` (e.g. information, trace) makes the backend push
    /// server-side log lines for the session's queries.
    const String logs_level = queryParam(uri, "logs", "", log);
    LOG_DEBUG(log, "WebSocket session established; output format {}", out_format);

    /// Bound each blocking WebSocket read so a stalled client cannot pin the
    /// handler thread indefinitely between queries.
    socket.setReceiveTimeout(Poco::Timespan(300, 0));

    ProxySession session(socket, context, backend, out_format, logs_level);
    session.run();

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
