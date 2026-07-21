#include <WsProxyHandler.h>

#include <Server/WebSocketFrames.h>
#include <Server/WebSocketSession.h>

#include <Client/Connection.h>
#include <Core/Protocol.h>
#include <Common/SSHWrapper.h>
#include <IO/ConnectionTimeouts.h>
#include <Interpreters/Context.h>
#include <Core/Settings.h>

#include <Server/HTTP/HTTPServerRequest.h>
#include <Server/HTTP/HTTPServerResponse.h>

#include <Common/Base64.h>
#include <Common/Exception.h>
#include <Common/logger_useful.h>

#include <IO/Operators.h>
#include <IO/WriteBuffer.h>

#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/URI.h>

#include <base/scope_guard.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <optional>
#include <utility>


namespace DB
{

using namespace DB::WsProxy;

namespace ErrorCodes
{
    extern const int BAD_ARGUMENTS;
}

namespace
{

/// Minimal JSON string escaping for a short error message embedded in a control frame.
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
String queryParam(const String & uri_string, const String & name, const String & fallback)
{
    Poco::URI uri(uri_string);
    for (const auto & param : uri.getQueryParameters())
    {
        if (param.first == name && !param.second.empty())
            return param.second;
    }
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

Int64 clientSendTimeoutSeconds()
{
    constexpr Int64 default_timeout = 30;
    constexpr Int64 max_timeout = 86'400;
    const char * value_string = std::getenv("WSPROXY_CLIENT_SEND_TIMEOUT_SEC");
    if (!value_string || !*value_string)
        return default_timeout;

    Int64 value = 0;
    const char * end = value_string + std::strlen(value_string);
    const auto parse_result = std::from_chars(value_string, end, value, 10);
    if (parse_result.ec != std::errc{} || parse_result.ptr != end || value <= 0 || value > max_timeout)
        throw Exception(
            ErrorCodes::BAD_ARGUMENTS,
            "`WSPROXY_CLIENT_SEND_TIMEOUT_SEC` must be an integer between 1 and {}",
            max_timeout);
    return value;
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
                LOG_WARNING(log, "WebSocket upgrade rejected: empty origin in `WSPROXY_ALLOWED_ORIGINS`");
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
                LOG_WARNING(log, "WebSocket upgrade rejected: malformed origin in `WSPROXY_ALLOWED_ORIGINS`");
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

/// Resolve backend credentials for this session (credential pass-through: the
/// backend performs authentication). Priority: `Authorization: Basic`, then
/// `X-ClickHouse-User`/`-Key` headers, then `?user=`/`?password=` URL params,
/// else the configured defaults already in `backend`.
void resolveCredentials(const HTTPServerRequest & request, const String & uri, BackendParams & backend)
{
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
            backend.user = decoded.substr(0, colon);
            backend.password = decoded.substr(colon + 1);
            return;
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
        backend.user = header_user;
        backend.password = request.get("X-ClickHouse-Key", "");
        return;
    }

    const String param_user = queryParam(uri, "user", "");
    if (!param_user.empty())
    {
        backend.user = param_user;
        backend.password = queryParam(uri, "password", "");
    }
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

    const char * allowed_origins_value = std::getenv("WSPROXY_ALLOWED_ORIGINS");
    const String allowed_origins = allowed_origins_value ? allowed_origins_value : "";
    if (!originAllowed(request, allowed_origins, log))
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
    BackendParams session_backend = backend;
    Int64 send_timeout_sec = 0;
    try
    {
        out_format = queryParam(uri, "format", "JSONEachRow");
        logs_level = queryParam(uri, "logs", "");
        flow_credit_param = positiveInt64QueryParam(uri, "flow");
        parse_param = queryParam(uri, "parse", "");
        parallel_param = queryParam(uri, "parallel", "");
        resolveCredentials(request, uri, session_backend);
        send_timeout_sec = clientSendTimeoutSeconds();
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

    /// Complete the handshake by writing 101 directly to the socket; from here
    /// on the stream is in WebSocket framing mode and we own the socket.
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

    /// Optional: `?logs=<level>` (e.g. information, trace) makes the backend push
    /// server-side log lines for the session's queries.
    /// Opt-in credit/window flow control: `?flow=N` enables it with N frames of
    /// initial credit; absent = push mode (unbounded). The client then grants more
    /// with {"cmd":"next","n":...} and can pause/resume.
    /// Opt-in SQL parsing: `?parse=1` lets the proxy parse each query to auto-route
    /// streamed-data INSERTs (no {"cmd":"insert"} needed) and report the parsed
    /// verb + routing decision as a {"event":"query",...} frame. Off by default —
    /// the proxy does not parse SQL unless the client explicitly asks it to.
    /// Opt-in parallel output formatting: `?parallel=1` formats SELECT output on a thread pool
    /// for higher conversion throughput, at the cost of coarser (batched) result frames. Ignored
    /// when flow control is on. Default off preserves fine-grained one-frame-per-block streaming.
    /// Credential pass-through: resolve this session's backend user/password from
    /// the request (never mutate the shared default `backend`).
    LOG_DEBUG(log, "WebSocket session established; format {}, backend user {}", out_format, session_backend.user);

    /// Bound each blocking WebSocket read so a stalled client cannot pin the
    /// handler thread indefinitely between queries.
    socket.setReceiveTimeout(Poco::Timespan(300, 0));

    /// Send timeout: drop a client that stops reading entirely (its TCP window
    /// stuck at 0) so a blocking write cannot pin a handler thread forever. The
    /// resulting throw routes through WriteBufferToWebSocket (broken -> sendCancel)
    /// to a clean teardown. Applies per blocking write, so a slow-but-progressing
    /// client is unaffected; only genuine zero-progress stalls trip it.
    /// Configurable (mainly for tests); default 30s.
    socket.setSendTimeout(Poco::Timespan(send_timeout_sec * 1'000'000)); /// microseconds

    /// Open one native-protocol connection per session to the remote backend.
    Connection connection(
        session_backend.host,
        session_backend.port,
        session_backend.database,
        session_backend.user,
        session_backend.password,
        /* proto_send_chunked_ */ "chunked_optional",
        /* proto_recv_chunked_ */ "chunked_optional",
        /* ssh_private_key_ */ SSHKey{},
        /* jwt_ */ "",
        /* quota_key_ */ "",
        /* cluster_ */ "",
        /* cluster_secret_ */ "",
        /* client_name_ */ "clickhouse-wsproxy",
        session_backend.compression_method == "none" ? Protocol::Compression::Disable : Protocol::Compression::Enable,
        session_backend.secure ? Protocol::Secure::Enable : Protocol::Secure::Disable,
        /* tls_sni_override_ */ "",
        /* bind_host_ */ "");

    /// Establish the backend connection up front so authentication (during the
    /// native handshake) is reported immediately rather than on the first query.
    /// On failure, tell the client and close.
    try
    {
        connection.forceConnected(ConnectionTimeouts::getTCPTimeoutsWithoutFailover(context->getSettingsRef()));
    }
    catch (...)
    {
        const String message = getCurrentExceptionMessage(false);
        LOG_DEBUG(log, "Backend connect/auth failed: {}", message);
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

    WebSocketSession session(
        socket, context, out_format, logs_level, flow_enabled, flow_credit, parse_enabled,
        parallel_enabled, session_backend.compression_method);
    session.run(connection);

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
