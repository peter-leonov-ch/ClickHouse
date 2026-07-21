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
#include <Poco/Util/LayeredConfiguration.h>

#include <base/scope_guard.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <optional>

namespace DB
{

using namespace DB::WsProxy;

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}

namespace
{

/// One resolved credential pair (delegated to ClickHouse for authN/authZ).
struct ResolvedCredentials
{
    String user = "default";
    String password;
};

String queryParam(const String & uri_string, const String & name, const String & fallback)
{
    Poco::URI uri(uri_string);
    for (const auto & param : uri.getQueryParameters())
        if (param.first == name && !param.second.empty())
            return param.second;
    return fallback;
}

std::optional<Int64> positiveInt64QueryParam(const String & uri_string, const String & name)
{
    Poco::URI uri(uri_string);
    std::optional<Int64> result;
    for (const auto & param : uri.getQueryParameters())
    {
        if (param.first != name)
            continue;

        if (result)
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Query parameter `{}` must not be repeated", name);

        Int64 value = 0;
        const char * begin = param.second.data();
        const char * end = begin + param.second.size();
        const auto parse_result = std::from_chars(begin, end, value, 10);
        if (param.second.empty() || parse_result.ec != std::errc{} || parse_result.ptr != end || value <= 0)
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Query parameter `{}` must be a positive integer", name);
        result = value;
    }
    return result;
}

String normalizeOrigin(const String & origin)
{
    Poco::URI uri(origin);
    String scheme = uri.getScheme();
    String host = uri.getHost();
    std::transform(scheme.begin(), scheme.end(), scheme.begin(), ::tolower);
    std::transform(host.begin(), host.end(), host.begin(), ::tolower);

    if ((scheme != "http" && scheme != "https") || host.empty() || !uri.getUserInfo().empty()
        || (!uri.getPath().empty() && uri.getPath() != "/") || !uri.getQuery().empty() || !uri.getFragment().empty())
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Origin header");

    const UInt16 port = uri.getPort();
    const UInt16 default_port = scheme == "https" ? 443 : 80;
    if (host.find(':') != String::npos)
        host = "[" + host + "]";
    return scheme + "://" + host + (port && port != default_port ? ":" + std::to_string(port) : "");
}

bool originAllowed(const HTTPServerRequest & request, const String & allowed_origins, LoggerPtr log)
{
    const String origin = request.get("Origin", "");
    if (origin.empty())
        return true;

    String normalized_origin;
    try
    {
        normalized_origin = normalizeOrigin(origin);
    }
    catch (...)
    {
        LOG_WARNING(log, "WebSocket upgrade rejected: malformed Origin header");
        return false;
    }

    if (!allowed_origins.empty())
    {
        bool allowed = false;
        size_t pos = 0;
        while (pos <= allowed_origins.size())
        {
            const size_t comma = allowed_origins.find(',', pos);
            const size_t end_pos = comma == String::npos ? allowed_origins.size() : comma;
            const size_t start = allowed_origins.find_first_not_of(" \t", pos);
            if (start == String::npos || start >= end_pos)
            {
                LOG_WARNING(log, "WebSocket upgrade rejected: empty origin in `ws_allowed_origins`");
                return false;
            }
            const size_t last = allowed_origins.find_last_not_of(" \t", end_pos - 1);
            try
            {
                if (normalizeOrigin(allowed_origins.substr(start, last - start + 1)) == normalized_origin)
                    allowed = true;
            }
            catch (...)
            {
                LOG_WARNING(log, "WebSocket upgrade rejected: malformed origin in `ws_allowed_origins`");
                return false;
            }
            if (comma == String::npos)
                break;
            pos = comma + 1;
        }
        return allowed;
    }

    try
    {
        const String request_scheme = request.isSecure() ? "https" : "http";
        return normalizeOrigin(request_scheme + "://" + request.getHost()) == normalized_origin;
    }
    catch (...)
    {
        LOG_WARNING(log, "WebSocket upgrade rejected: malformed Host header");
        return false;
    }
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
ResolvedCredentials resolveCredentials(const HTTPServerRequest & request, const String & uri)
{
    ResolvedCredentials creds;

    const String auth = request.get("Authorization", "");
    const size_t auth_scheme_end = auth.find_first_of(" \t");
    String auth_scheme = auth.substr(0, auth_scheme_end);
    std::transform(auth_scheme.begin(), auth_scheme.end(), auth_scheme.begin(), ::tolower);
    if (auth_scheme == "basic")
    {
        try
        {
            if (auth_scheme_end != 5 || auth.size() <= 6 || auth[5] != ' ')
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Basic Authorization header");
            const String encoded = auth.substr(6);
            const size_t padding_pos = encoded.find('=');
            const size_t data_end = padding_pos == String::npos ? encoded.size() : padding_pos;
            if (encoded.size() % 4 != 0 || encoded.size() - data_end > 2)
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Basic Authorization header");
            for (size_t i = 0; i < encoded.size(); ++i)
            {
                const unsigned char c = encoded[i];
                const bool is_base64_character
                    = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '+' || c == '/';
                if (i < data_end ? !is_base64_character : c != '=')
                    throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Basic Authorization header");
            }
            const String decoded = base64Decode(encoded);
            const size_t colon = decoded.find(':');
            if (colon == String::npos || colon == 0)
                throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Basic Authorization header");
            creds.user = decoded.substr(0, colon);
            creds.password = decoded.substr(colon + 1);
            return creds;
        }
        catch (...)
        {
            throw Exception(ErrorCodes::BAD_ARGUMENTS, "Malformed Basic Authorization header");
        }
    }
    if (!auth.empty())
        throw Exception(ErrorCodes::BAD_ARGUMENTS, "Unsupported Authorization scheme");

    const String header_user = request.get("X-ClickHouse-User", "");
    if (!header_user.empty())
    {
        creds.user = header_user;
        creds.password = request.get("X-ClickHouse-Key", "");
        return creds;
    }

    const String param_user = queryParam(uri, "user", "");
    if (!param_user.empty())
    {
        creds.user = param_user;
        creds.password = queryParam(uri, "password", "");
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

    if (!originAllowed(request, server.config().getString("ws_allowed_origins", ""), log))
    {
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
        *response.send() << "Origin not allowed.\n";
        return;
    }

    const String & uri = request.getURI();
    String out_format;
    String logs_level;
    std::optional<Int64> flow_credit_param;
    String parse_param;
    String parallel_param;
    ResolvedCredentials creds;
    try
    {
        out_format = queryParam(uri, "format", "JSONEachRow");
        logs_level = queryParam(uri, "logs", "");
        flow_credit_param = positiveInt64QueryParam(uri, "flow");
        parse_param = queryParam(uri, "parse", "");
        parallel_param = queryParam(uri, "parallel", "");
        creds = resolveCredentials(request, uri);
    }
    catch (...)
    {
        LOG_DEBUG(log, "Invalid WebSocket request: {}", getCurrentExceptionMessage(false));
        response.setStatusAndReason(Poco::Net::HTTPResponse::HTTP_BAD_REQUEST);
        *response.send() << getCurrentExceptionMessage(false) << "\n";
        return;
    }

    const bool flow_enabled = flow_credit_param.has_value();
    const Int64 flow_credit = flow_credit_param.value_or(0);
    const bool parse_enabled = parse_param == "1" || parse_param == "true";
    const bool parallel_enabled = parallel_param == "1" || parallel_param == "true";

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

    /// Bound each blocking read; bound blocking writes so a client that stops
    /// reading cannot pin a handler thread (mirrors the standalone proxy).
    socket.setReceiveTimeout(Poco::Timespan(300, 0));
    socket.setSendTimeout(Poco::Timespan(30, 0));

    /// Authenticate against the server (ClickHouse performs authN/authZ) and
    /// build an in-process connection bound to that session.
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
