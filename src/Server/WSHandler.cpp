#include <Server/WSHandler.h>

#include <Server/WebSocketFrames.h>
#include <Server/WebSocketSession.h>

#include <Server/IServer.h>
#include <Server/HTTP/HTTPServerRequest.h>
#include <Server/HTTP/HTTPServerResponse.h>

#include <Client/LocalConnection.h>
#include <Interpreters/Session.h>
#include <Interpreters/Context.h>
#include <Access/Credentials.h>

#include <Common/Base64.h>
#include <Common/Exception.h>
#include <Common/logger_useful.h>

#include <IO/Operators.h>

#include <Poco/Net/HTTPRequest.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/URI.h>

#include <base/scope_guard.h>

#include <algorithm>

namespace DB
{

using namespace DB::WsProxy;

namespace
{

/// One resolved credential pair (delegated to ClickHouse for authN/authZ).
struct ResolvedCredentials
{
    String user = "default";
    String password;
};

String queryParam(const String & uri_string, const String & name, const String & fallback, LoggerPtr log)
{
    try
    {
        Poco::URI uri(uri_string);
        for (const auto & param : uri.getQueryParameters())
            if (param.first == name && !param.second.empty())
                return param.second;
    }
    catch (...)
    {
        LOG_DEBUG(log, "Could not parse request URI for the {} parameter; using default", name);
    }
    return fallback;
}

/// Per RFC 7230 the `Connection` header is a comma-separated token list; match the
/// exact `upgrade` token rather than a substring.
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

/// Minimal JSON string escaping for a short error message in a control frame.
String jsonEscape(const String & s)
{
    String out;
    out.reserve(s.size() + 2);
    for (char c : s)
    {
        switch (c)
        {
            case '"': out += R"(\")"; break;
            case '\\': out += R"(\\)"; break;
            case '\n': out += R"(\n)"; break;
            case '\r': out += R"(\r)"; break;
            case '\t': out += R"(\t)"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    static const char * hex = "0123456789abcdef";
                    out += R"(\u00)";
                    out += hex[(c >> 4) & 0xF];
                    out += hex[c & 0xF];
                }
                else
                    out += c;
        }
    }
    return out;
}

/// Resolve credentials the same way clickhouse-wsproxy does, so the same clients
/// work against either deployment: Authorization: Basic > X-ClickHouse-User/-Key
/// headers > ?user=/?password= URL params > the `default` user.
ResolvedCredentials resolveCredentials(const HTTPServerRequest & request, const String & uri, LoggerPtr log)
{
    ResolvedCredentials creds;

    const String auth = request.get("Authorization", "");
    if (auth.starts_with("Basic "))
    {
        try
        {
            const String decoded = base64Decode(auth.substr(6));
            const size_t colon = decoded.find(':');
            if (colon != String::npos)
            {
                creds.user = decoded.substr(0, colon);
                creds.password = decoded.substr(colon + 1);
                return creds;
            }
        }
        catch (...)
        {
            LOG_DEBUG(log, "Malformed Authorization header; falling back to other credential sources");
        }
    }

    const String header_user = request.get("X-ClickHouse-User", "");
    if (!header_user.empty())
    {
        creds.user = header_user;
        creds.password = request.get("X-ClickHouse-Key", "");
        return creds;
    }

    const String param_user = queryParam(uri, "user", "", log);
    if (!param_user.empty())
    {
        creds.user = param_user;
        creds.password = queryParam(uri, "password", "", log);
    }
    return creds;
}

}

WSHandler::WSHandler(IServer & server_) : server(server_)
{
}

void WSHandler::handleRequest(HTTPServerRequest & request, HTTPServerResponse & response, const ProfileEvents::Event &)
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
        tryLogCurrentException("WSHandler");
    }
}

void WSHandler::serveInfo(HTTPServerRequest & request, HTTPServerResponse & response)
{
    response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_OK);
    response.setContentType("text/plain; charset=UTF-8");
    *response.send()
        << "ClickHouse WebSocket endpoint\n"
        << "Requested: " << request.getURI() << "\n"
        << "Open a WebSocket connection to run queries. Send a query as a text\n"
        << "frame; results stream back as binary frames in the output format\n"
        << "(set via ?format=..., default JSONEachRow), followed by a JSON\n"
        << "control frame ({\"event\":\"end\"} / \"error\" / \"cancelled\").\n";
}

void WSHandler::handleWebSocket(HTTPServerRequest & request, HTTPServerResponse & response)
{
    LoggerPtr log = getLogger("WSHandler");

    if (request.getMethod() != Poco::Net::HTTPRequest::HTTP_GET)
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
    if (request.get("Sec-WebSocket-Version", "") != "13")
    {
        response.set("Sec-WebSocket-Version", "13");
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        *response.send() << "Unsupported WebSocket version.\n";
        return;
    }

    /// Complete the handshake by writing 101 directly to the socket; from here on
    /// the stream is in WebSocket framing mode and we own the socket.
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
            LOG_DEBUG(log, "WebSocket socket shutdown: {}", getCurrentExceptionMessage(false));
        }
    });

    const String & uri = request.getURI();
    const String out_format = queryParam(uri, "format", "JSONEachRow", log);
    const String logs_level = queryParam(uri, "logs", "", log);
    const String flow_param = queryParam(uri, "flow", "", log);
    const bool flow_enabled = !flow_param.empty();
    const Int64 flow_credit = flow_enabled ? std::strtoll(flow_param.c_str(), nullptr, 10) : 0;
    const String parse_param = queryParam(uri, "parse", "", log);
    const bool parse_enabled = parse_param == "1" || parse_param == "true";
    const String parallel_param = queryParam(uri, "parallel", "", log);
    const bool parallel_enabled = parallel_param == "1" || parallel_param == "true";

    /// Bound each blocking read; bound blocking writes so a client that stops
    /// reading cannot pin a handler thread (mirrors the standalone proxy).
    socket.setReceiveTimeout(Poco::Timespan(300, 0));
    socket.setSendTimeout(Poco::Timespan(30, 0));

    /// Authenticate against the server (ClickHouse performs authN/authZ) and
    /// build an in-process connection bound to that session.
    const ResolvedCredentials creds = resolveCredentials(request, uri, log);
    ContextMutablePtr session_context;
    std::unique_ptr<LocalConnection> connection;
    try
    {
        auto session = std::make_unique<Session>(server.context(), ClientInfo::Interface::HTTP, request.isSecure());
        session->authenticate(BasicCredentials(creds.user, creds.password), request.clientAddress());
        session_context = session->makeSessionContext();
        /// LocalConnection ignores the per-query settings passed to sendQuery and reads them from
        /// its context, so apply the session-level knobs here. send_logs_level drives whether the
        /// in-process query pushes Log packets (which WebSocketSession forwards as `log` frames).
        if (!logs_level.empty())
            session_context->setSetting("send_logs_level", logs_level);
        connection = std::make_unique<LocalConnection>(
            std::move(session), /* in */ nullptr, /* send_progress */ true, /* send_profile_events */ true, /* server_display_name */ "");
    }
    catch (...)
    {
        const String message = getCurrentExceptionMessage(false);
        LOG_DEBUG(log, "WebSocket authentication failed: {}", message);
        try
        {
            sendWebSocketText(socket, R"({"event":"error","message":")" + jsonEscape(message) + "\"}");
            sendWebSocketClose(socket, /* 1008 policy violation */ 1008, "Authentication failed");
        }
        catch (...)
        {
            LOG_DEBUG(log, "Failed to deliver auth error to client (already gone)");
        }
        return;
    }

    LOG_DEBUG(log, "WebSocket session established for user {}; format {}", creds.user, out_format);

    WebSocketSession session(
        socket, session_context, out_format, logs_level, flow_enabled, flow_credit, parse_enabled, parallel_enabled);
    session.run(*connection);

    LOG_DEBUG(log, "WebSocket session closed");
}

}
