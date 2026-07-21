#include <Server/WebSocketSession.h>
#include <Server/WebSocketFrames.h>

#include <Client/IServerConnection.h>

#include <Core/Block.h>
#include <Core/Defines.h>
#include <Core/Protocol.h>
#include <Core/QueryProcessingStage.h>
#include <Core/Settings.h>

#include <Interpreters/ClientInfo.h>
#include <Interpreters/Context.h>

#include <Parsers/ASTInsertQuery.h>
#include <Parsers/ParserQuery.h>
#include <Parsers/parseQuery.h>
#include <Parsers/Lexer.h>

#include <atomic>
#include <ctime>
#include <limits>
#include <mutex>

#include <Formats/FormatFactory.h>
#include <Processors/Formats/IOutputFormat.h>
#include <Processors/Executors/PullingPipelineExecutor.h>
#include <QueryPipeline/Pipe.h>
#include <QueryPipeline/QueryPipeline.h>

#include <IO/BufferWithOwnMemory.h>
#include <IO/ConnectionTimeouts.h>
#include <IO/ReadBuffer.h>
#include <IO/WriteBuffer.h>
#include <IO/WriteBufferFromString.h>

#include <Common/Exception.h>
#include <Common/SSHWrapper.h>
#include <Common/isValidUTF8.h>
#include <Common/logger_useful.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Parser.h>


namespace DB
{

using namespace DB::WsProxy;

namespace
{

/// A parsed `{"cmd":"insert",...}` control message (a streamed data INSERT).
struct InsertCommand
{
    bool is_insert = false;
    String query;
    String format; /// Input format for the client's streamed data; empty = use session default.
};

/// Detect and parse a streamed-insert control message WITHOUT touching SQL. Only
/// a top-level JSON object with `"cmd":"insert"` is an insert; anything else is a
/// plain query. (SQL never starts with '{', so the check is unambiguous.)
InsertCommand parseInsertCommand(const String & message)
{
    InsertCommand cmd;
    const size_t first = message.find_first_not_of(" \t\r\n");
    if (first == String::npos || message[first] != '{')
        return cmd; /// Not JSON -> a plain query.
    try
    {
        Poco::JSON::Parser parser;
        const auto obj = parser.parse(message).extract<Poco::JSON::Object::Ptr>();
        if (obj->optValue<String>("cmd", "") != "insert")
            return cmd;
        cmd.is_insert = true;
        cmd.query = obj->optValue<String>("query", "");
        cmd.format = obj->optValue<String>("format", "");
    }
    catch (...)
    {
        cmd.is_insert = false; /// Malformed -> treat as a plain query; the backend reports errors.
    }
    return cmd;
}

/// The leading SQL keyword of a query (e.g. `SELECT`, `INSERT`, `CREATE`),
/// uppercased. Uses the SQL lexer so leading comments/whitespace are skipped
/// correctly. Clients frequently want this to drive their own logic without
/// re-implementing a tokenizer. Returns empty if there is no significant token.
String leadingVerb(const String & query)
{
    Lexer lexer(query.data(), query.data() + query.size());
    for (Token token = lexer.nextToken(); !token.isEnd() && !token.isError(); token = lexer.nextToken())
    {
        if (!token.isSignificant())
            continue;
        String verb(token.begin, token.size());
        for (char & c : verb)
            if (c >= 'a' && c <= 'z')
                c = static_cast<char>(c - ('a' - 'A'));
        return verb;
    }
    return "";
}

/// The proxy's classification of a query, reported to the client in ?parse=1 mode.
struct QueryClass
{
    String verb;                  /// Leading SQL keyword, uppercased.
    bool streamed_insert = false; /// True if it is an INSERT that expects client-streamed data.
    String insert_format;         /// The FORMAT clause of such an INSERT (empty = session default).
};

/// OPT-IN only (?parse=1): parse the query so the proxy can (a) report the leading
/// verb + routing decision to the client and (b) auto-route a streamed-data INSERT
/// without the client sending an explicit {"cmd":"insert",...} envelope.
///
/// A query needs the client-streamed data phase iff it is an `INSERT` with no
/// SELECT source, no INFILE, and no inline data (`INSERT INTO t [FORMAT X]` with
/// the rows still to come). Everything else (SELECT / INSERT-SELECT / inline
/// VALUES / DDL) is a plain query.
///
/// This is the ONLY place the proxy parses SQL, and only when the client asks for
/// it. A parse failure is returned to the client instead of silently changing the
/// routing decision.
QueryClass classifyQuery(const String & query)
{
    QueryClass result;
    result.verb = leadingVerb(query);

    ParserQuery parser(query.data() + query.size());
    ASTPtr ast = parseQuery(
        parser,
        query,
        /* max_query_size */ 0, /// 0 = unlimited; the backend enforces the real limit.
        /* max_parser_depth */ 1000,
        /* max_parser_backtracks */ 1'000'000);

    if (const auto * insert = ast->as<ASTInsertQuery>())
    {
        const bool has_inline_data = insert->data != nullptr && insert->data != insert->end;
        result.streamed_insert = !insert->select && !insert->infile && !has_inline_data;
        result.insert_format = insert->format;
    }
    return result;
}

constexpr UInt64 MAX_CLIENT_MESSAGE_SIZE = 16 * 1024 * 1024;
constexpr UInt64 MID_QUERY_FRAME_READ_TIMEOUT_NS = 1'000'000'000;
constexpr UInt64 CLIENT_FRAME_READ_TIMEOUT_NS = 30'000'000'000;

bool isValidUTF8(const String & value)
{
    return UTF8::isValidUTF8(reinterpret_cast<const UInt8 *>(value.data()), value.size());
}

/// Opt-in credit/window flow control for the SELECT push direction. The client
/// grants credit in whole frames (`{"cmd":"next","n":N}`) and can `pause`/`resume`;
/// the proxy sends a data frame only while `!paused && credit > 0`. This lets an
/// event-based JS client (which cannot apply receive backpressure) bound how much
/// it must buffer. `client_gone` latches a Close/broken read.
struct FlowControl
{
    bool enabled = false;
    Int64 credit = 0;
    bool paused = false;
    std::atomic<bool> client_gone = false;
    bool fragmented_message = false;
    uint8_t fragmented_opcode = 0;
    String fragmented_payload;
};

UInt64 monotonicNs()
{
    struct timespec ts
    {
    };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<UInt64>(ts.tv_sec) * 1'000'000'000ULL + static_cast<UInt64>(ts.tv_nsec);
}

/// Apply one client control frame to the flow state (also answers pings). Used
/// both by the between-packets poll and by the credit gate, so `next`/`pause`/
/// `resume`/close are handled consistently wherever the client frame is read.
/// `write_mutex` (if given) serializes the Pong send against other socket writers
/// (the parallel-format collector thread).
void applyControlFrame(
    Poco::Net::StreamSocket & socket, const WebSocketFrame & frame, FlowControl & fc, std::mutex * write_mutex = nullptr)
{
    auto send_close = [&](uint16_t code, const String & reason)
    {
        fc.client_gone = true;
        try
        {
            std::unique_lock<std::mutex> lock;
            if (write_mutex)
                lock = std::unique_lock<std::mutex>(*write_mutex);
            sendWebSocketClose(socket, code, reason);
        }
        catch (...)
        {
        }
    };

    if (frame.protocol_error)
    {
        send_close(1002, "Protocol error");
        return;
    }
    if (frame.message_too_big)
    {
        send_close(1009, "Message too big");
        return;
    }
    if (frame.invalid_utf8)
    {
        send_close(1007, "Invalid UTF-8");
        return;
    }
    if (!frame.valid)
    {
        fc.client_gone = true;
        return;
    }
    if (frame.opcode == Opcode::Close)
    {
        fc.client_gone = true;
        try
        {
            std::unique_lock<std::mutex> lock;
            if (write_mutex)
                lock = std::unique_lock<std::mutex>(*write_mutex);
            sendWebSocketFrame(socket, Opcode::Close, frame.payload.data(), frame.payload.size());
        }
        catch (...)
        {
        }
        return;
    }
    if (frame.opcode == Opcode::Ping)
    {
        try
        {
            std::unique_lock<std::mutex> lock;
            if (write_mutex)
                lock = std::unique_lock<std::mutex>(*write_mutex);
            sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
        }
        catch (...)
        {
            fc.client_gone = true;
        }
        return;
    }

    uint8_t message_opcode = frame.opcode;
    String message_payload;
    if (frame.opcode == Opcode::Continuation)
    {
        if (!fc.fragmented_message)
        {
            send_close(1002, "Protocol error");
            return;
        }
        if (frame.payload.size() > MAX_CLIENT_MESSAGE_SIZE - fc.fragmented_payload.size())
        {
            send_close(1009, "Message too big");
            return;
        }
        fc.fragmented_payload.append(frame.payload);
        if (!frame.fin)
            return;

        message_opcode = fc.fragmented_opcode;
        message_payload = std::move(fc.fragmented_payload);
        fc.fragmented_message = false;
        fc.fragmented_opcode = 0;
    }
    else
    {
        if (fc.fragmented_message)
        {
            send_close(1002, "Protocol error");
            return;
        }
        if (!frame.fin)
        {
            fc.fragmented_message = true;
            fc.fragmented_opcode = frame.opcode;
            fc.fragmented_payload = frame.payload;
            return;
        }
        message_payload = frame.payload;
    }

    if (message_opcode != Opcode::Text)
        return; /// Ignore stray binary messages during a SELECT.
    if (!isValidUTF8(message_payload))
    {
        send_close(1007, "Invalid UTF-8");
        return;
    }

    bool next_command = false;
    try
    {
        Poco::JSON::Parser parser;
        const auto obj = parser.parse(message_payload).extract<Poco::JSON::Object::Ptr>();
        const String cmd = obj->optValue<String>("cmd", "");
        if (cmd == "next")
        {
            next_command = true;
            const Int64 grant = obj->optValue<Poco::Int64>("n", 0);
            if (grant <= 0 || fc.credit < 0 || grant > std::numeric_limits<Int64>::max() - fc.credit)
            {
                send_close(1008, "Invalid flow-control credit");
                return;
            }
            fc.credit += grant;
        }
        else if (cmd == "pause")
            fc.paused = true;
        else if (cmd == "resume")
            fc.paused = false;
    }
    catch (...)
    {
        if (next_command)
        {
            send_close(1008, "Invalid flow-control credit");
            return;
        }
        /// Malformed control frame: ignore (the query stream is unaffected).
        LOG_DEBUG(getLogger("WebSocketSession"), "Ignoring malformed control frame: {}", getCurrentExceptionMessage(false));
    }
}

/// A `WriteBuffer` whose flushes are emitted as WebSocket binary frames. The
/// output format writes into it directly, so calling `flush` after each result
/// block streams that block to the client as its own frame (mid-query push).
///
/// Send failures are swallowed and latched into `broken` rather than thrown:
/// once the client is gone there is nothing to do but stop, and a throwing
/// `nextImpl` would otherwise leave the buffer unfinalized (and could throw
/// from `finalize`). Callers check `isBroken` to notice the client left.
class WriteBufferToWebSocket : public BufferWithOwnMemory<WriteBuffer>
{
public:
    explicit WriteBufferToWebSocket(
        Poco::Net::StreamSocket & socket_,
        FlowControl * flow_ = nullptr,
        std::mutex * write_mutex_ = nullptr,
        size_t size = DBMS_DEFAULT_BUFFER_SIZE)
        : BufferWithOwnMemory<WriteBuffer>(size)
        , socket(socket_)
        , flow(flow_)
        , write_mutex(write_mutex_)
    {
    }

    ~WriteBufferToWebSocket() override = default;

    bool isBroken() const { return broken; }

private:
    void nextImpl() override
    {
        if (broken || offset() == 0)
            return;

        /// Flow control: block this data frame until the client has granted credit
        /// (and is not paused). While blocked we read the client's control frames
        /// (next/pause/resume, or a Close), which also stops us reading the backend
        /// -> TCP backpressure to the server, paced by the client's consumption.
        if (flow && flow->enabled)
        {
            while (!flow->client_gone && (flow->paused || flow->credit <= 0))
            {
                WebSocketFrame frame;
                try
                {
                    frame = readWebSocketFrame(
                        socket, /* deadline_ns */ 0, MAX_CLIENT_MESSAGE_SIZE, MID_QUERY_FRAME_READ_TIMEOUT_NS);
                }
                catch (...)
                {
                    flow->client_gone = true;
                    break;
                }
                applyControlFrame(socket, frame, *flow, write_mutex);
            }
            if (flow->client_gone)
            {
                broken = true;
                return;
            }
        }

        try
        {
            /// The send must be atomic against the session thread's control-frame
            /// writes (used when parallel formatting runs this on a collector thread).
            std::unique_lock<std::mutex> lock;
            if (write_mutex)
                lock = std::unique_lock<std::mutex>(*write_mutex);
            if (flow && flow->client_gone)
            {
                broken = true;
                return;
            }
            sendWebSocketBinary(socket, working_buffer.begin(), offset());
        }
        catch (...)
        {
            broken = true;
            return;
        }

        if (flow && flow->enabled)
            --flow->credit;
    }

    Poco::Net::StreamSocket & socket;
    FlowControl * flow;
    std::mutex * write_mutex;
    std::atomic<bool> broken = false;
};

/// A `ReadBuffer` that yields the payloads of incoming WebSocket binary frames,
/// used to feed the input format when parsing client-supplied INSERT data.
///
/// End of data is a zero-length binary frame or a text frame (clean). A Close
/// frame or read error is an abort (client disconnected mid-insert): `wasAborted`
/// lets the caller cancel the backend query instead of committing partial data.
class ReadBufferFromWebSocket : public ReadBuffer
{
public:
    explicit ReadBufferFromWebSocket(Poco::Net::StreamSocket & socket_)
        : ReadBuffer(nullptr, 0), socket(socket_)
    {
    }

    bool wasAborted() const { return aborted; }

private:
    bool fail(uint16_t code, const String & reason)
    {
        aborted = true;
        try
        {
            sendWebSocketClose(socket, code, reason);
        }
        catch (...)
        {
        }
        return false;
    }

    bool nextImpl() override
    {
        /// `ReadBuffer::next` may call `nextImpl` again after a false return
        /// (there is no permanent EOF latch in the base class), and some input
        /// formats do a trailing read. Latch the end so we never block on a
        /// frame that will not arrive.
        if (finished || aborted)
            return false;

        while (true)
        {
            WebSocketFrame frame;
            try
            {
                frame = readWebSocketFrame(
                    socket, /* deadline_ns */ 0, MAX_CLIENT_MESSAGE_SIZE, CLIENT_FRAME_READ_TIMEOUT_NS);
            }
            catch (...)
            {
                aborted = true;
                return false;
            }

            if (frame.protocol_error)
                return fail(1002, "Protocol error");
            if (frame.message_too_big)
                return fail(1009, "Message too big");
            if (frame.invalid_utf8)
                return fail(1007, "Invalid UTF-8");
            if (!frame.valid)
            {
                aborted = true;
                return false;
            }
            if (frame.opcode == Opcode::Close)
            {
                aborted = true;
                try
                {
                    sendWebSocketFrame(socket, Opcode::Close, frame.payload.data(), frame.payload.size());
                }
                catch (...)
                {
                }
                return false;
            }
            if (frame.opcode == Opcode::Ping)
            {
                try
                {
                    sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
                }
                catch (...)
                {
                    aborted = true;
                    return false;
                }
                continue;
            }

            uint8_t message_opcode = frame.opcode;
            String message_payload;
            if (frame.opcode == Opcode::Continuation)
            {
                if (!fragmented_message)
                    return fail(1002, "Protocol error");
                if (frame.payload.size() > MAX_CLIENT_MESSAGE_SIZE - fragmented_payload.size())
                    return fail(1009, "Message too big");
                fragmented_payload.append(frame.payload);
                if (!frame.fin)
                    continue;

                message_opcode = fragmented_opcode;
                message_payload = std::move(fragmented_payload);
                fragmented_message = false;
                fragmented_opcode = 0;
            }
            else
            {
                if (fragmented_message)
                    return fail(1002, "Protocol error");
                if (!frame.fin)
                {
                    fragmented_message = true;
                    fragmented_opcode = frame.opcode;
                    fragmented_payload = frame.payload;
                    continue;
                }
                message_payload = std::move(frame.payload);
            }

            if (message_opcode == Opcode::Text)
            {
                if (!isValidUTF8(message_payload))
                    return fail(1007, "Invalid UTF-8");
                finished = true; /// Clean end-of-data control frame.
                return false;
            }
            if (message_opcode != Opcode::Binary)
                return fail(1002, "Protocol error");

            /// An empty binary message is the clean end marker.
            if (message_payload.empty())
            {
                finished = true;
                return false;
            }

            current_frame = std::move(message_payload);
            BufferBase::set(current_frame.data(), current_frame.size(), 0);
            return true;
        }
    }

    Poco::Net::StreamSocket & socket;
    String current_frame;
    String fragmented_payload;
    uint8_t fragmented_opcode = 0;
    bool fragmented_message = false;
    bool aborted = false;
    bool finished = false;
};

/// Minimal JSON string escaping for the small control-frame payloads.
String escapeJSON(const String & s)
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

/// Report the proxy's parse decision to the client (only in ?parse=1 mode), as a
/// non-terminal control frame preceding the result. `kind` is the routing
/// decision: "insert" = the proxy will read client-streamed data; "query" = the
/// plain path (results / self-contained statement). `verb` is the leading keyword.
void sendQueryInfoFrame(Poco::Net::StreamSocket & socket, const String & verb, const String & kind)
{
    sendWebSocketText(socket, R"({"event":"query","kind":")" + kind + R"(","verb":")" + escapeJSON(verb) + R"("})");
}

}

WebSocketSession::WebSocketSession(
    Poco::Net::StreamSocket & socket_,
    ContextPtr context_,
    String format_,
    String logs_level_,
    bool flow_enabled_,
    Int64 flow_initial_credit_,
    bool parse_enabled_,
    bool parallel_enabled_,
    String compression_method_)
    : socket(socket_)
    , context(std::move(context_))
    , format(std::move(format_))
    , logs_level(std::move(logs_level_))
    , flow_enabled(flow_enabled_)
    , flow_initial_credit(flow_initial_credit_)
    , parse_enabled(parse_enabled_)
    , parallel_enabled(parallel_enabled_)
    , compression_method(std::move(compression_method_))
{
}

void WebSocketSession::sendControlEvent(const String & event, const String & message)
{
    String json = R"({"event":")" + event + "\"";
    if (!message.empty())
        json += R"(,"message":")" + escapeJSON(message) + "\"";
    json += "}";
    std::lock_guard lock(ws_write_mutex);
    sendWebSocketText(socket, json);
}

void WebSocketSession::sendBlockEvent(const String & event, const Block & block)
{
    if (block.rows() == 0)
        return;

    /// Serialize the block to JSONEachRow (one JSON object per row), then splice
    /// the rows into a JSON array so the whole batch is one valid control frame:
    /// {"event":"log","rows":[{...},{...}]}. Reusing the output format keeps this
    /// correct for whatever columns/types the server sends.
    WriteBufferFromOwnString buf;
    auto out = FormatFactory::instance().getOutputFormat("JSONEachRow", buf, block.cloneEmpty(), context);
    out->write(materializeBlock(block, !out->supportsSpecialSerializationKinds()));
    out->finalize();
    const String & rows_text = buf.str();

    String rows_array;
    size_t start = 0;
    bool first = true;
    while (start < rows_text.size())
    {
        const size_t newline = rows_text.find('\n', start);
        const size_t end = (newline == String::npos) ? rows_text.size() : newline;
        if (end > start)
        {
            if (!first)
                rows_array += ',';
            rows_array.append(rows_text, start, end - start);
            first = false;
        }
        if (newline == String::npos)
            break;
        start = newline + 1;
    }

    std::lock_guard lock(ws_write_mutex);
    sendWebSocketText(socket, R"({"event":")" + event + R"(","rows":[)" + rows_array + "]}");
}

void WebSocketSession::sendBackendQuery(IServerConnection & connection, const String & query, bool with_pending_data)
{
    /// Copy the settings so we can opt into server log delivery. The backend
    /// attaches its log queue based on `send_logs_level` in the query packet's
    /// settings (a query-text SETTINGS clause is too late), so it must be set here.
    Settings settings = context->getSettingsRef();
    if (!logs_level.empty())
        settings.set("send_logs_level", logs_level);

    /// The server compresses the result blocks it sends using the client's
    /// `network_compression_method`. On a bandwidth-limited WAN this codec choice
    /// dominates end-to-end time; ZSTD's higher ratio (columnar native compresses
    /// far better than row JSON) makes the proxy beat gzipped HTTP. Empty = leave
    /// the backend/connection default (LZ4). "none" disables block compression via
    /// the connection's compression flag, so nothing to set here.
    if (compression_method != "none" && !compression_method.empty())
        settings.set("network_compression_method", compression_method);

    auto timeouts = ConnectionTimeouts::getTCPTimeoutsWithoutFailover(settings);

    /// Pass no ClientInfo: a remote Connection then lets the server default the
    /// query kind for a direct client, and an in-process LocalConnection derives
    /// the query context (and current user) from its authenticated session rather
    /// than being clobbered by an empty ClientInfo.
    /// `with_pending_data` must be true for INSERTs so the connection enters the
    /// send-data phase and replies with the sample/header block.
    connection.sendQuery(
        timeouts,
        query,
        /* query_parameters */ {},
        /* query_id */ "",
        QueryProcessingStage::Complete,
        &settings,
        /* client_info */ nullptr,
        with_pending_data,
        /* external_roles */ {},
        /* process_progress_callback */ {});
}

void WebSocketSession::drainUntilEndOfStream(IServerConnection & connection)
{
    try
    {
        while (true)
        {
            Packet packet = connection.receivePacket();
            if (packet.type == Protocol::Server::EndOfStream || packet.type == Protocol::Server::Exception)
                return;
        }
    }
    catch (...)
    {
        /// The connection may be broken after a cancel; nothing more to drain.
        LOG_DEBUG(getLogger("WebSocketSession"), "drain after cancel: {}", getCurrentExceptionMessage(false));
    }
}

std::optional<String> WebSocketSession::readClientMessage()
{
    String buffer;
    bool in_fragmented_message = false;
    uint8_t message_opcode = 0;

    auto send_close = [&](uint16_t code, const String & reason)
    {
        try
        {
            std::lock_guard lock(ws_write_mutex);
            sendWebSocketClose(socket, code, reason);
        }
        catch (...)
        {
        }
    };

    while (true)
    {
        WebSocketFrame frame;
        try
        {
            frame = readWebSocketFrame(
                socket, /* deadline_ns */ 0, MAX_CLIENT_MESSAGE_SIZE, CLIENT_FRAME_READ_TIMEOUT_NS);
        }
        catch (...)
        {
            return std::nullopt;
        }

        if (frame.protocol_error)
        {
            send_close(1002, "Protocol error");
            return std::nullopt;
        }
        if (frame.message_too_big)
        {
            send_close(1009, "Message too big");
            return std::nullopt;
        }
        if (frame.invalid_utf8)
        {
            send_close(1007, "Invalid UTF-8");
            return std::nullopt;
        }
        if (!frame.valid)
            return std::nullopt;

        /// Control frames may interleave with data frames.
        if (frame.opcode >= 0x08)
        {
            if (frame.opcode == Opcode::Close)
            {
                try
                {
                    std::lock_guard lock(ws_write_mutex);
                    sendWebSocketFrame(socket, Opcode::Close, frame.payload.data(), frame.payload.size());
                }
                catch (...)
                {
                }
                return std::nullopt;
            }
            if (frame.opcode == Opcode::Ping)
            {
                try
                {
                    std::lock_guard lock(ws_write_mutex);
                    sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
            continue;
        }

        if (frame.opcode != Opcode::Continuation)
        {
            if (in_fragmented_message)
            {
                send_close(1002, "Protocol error");
                return std::nullopt; /// New data frame during fragmentation.
            }
            buffer = std::move(frame.payload);
            message_opcode = frame.opcode;
            in_fragmented_message = !frame.fin;
        }
        else
        {
            if (!in_fragmented_message)
            {
                send_close(1002, "Protocol error");
                return std::nullopt; /// Continuation without a start.
            }
            if (frame.payload.size() > MAX_CLIENT_MESSAGE_SIZE - buffer.size())
            {
                send_close(1009, "Message too big");
                return std::nullopt;
            }
            buffer.append(frame.payload);
            if (frame.fin)
                in_fragmented_message = false;
        }

        if (frame.fin)
        {
            if (message_opcode == Opcode::Text && !isValidUTF8(buffer))
            {
                send_close(1007, "Invalid UTF-8");
                return std::nullopt;
            }
            return buffer;
        }
    }
}

bool WebSocketSession::executeSelect(IServerConnection & connection, const String & query)
{
    LoggerPtr log = getLogger("WebSocketSession");

    sendBackendQuery(connection, query);

    FlowControl fc;
    fc.enabled = flow_enabled;
    fc.credit = flow_initial_credit;

    /// Parallel output formatting (opt-in, ?parallel=1) spreads the otherwise single-threaded
    /// (~500 MB/s) format work across cores. It runs a collector thread that writes result frames,
    /// so all frame sends are serialized behind `ws_write_mutex`. Trade-off: result frames are
    /// coarser (blocks are batched, not one frame per block). It is incompatible with flow control
    /// (whose credit gate reads the client socket, which must stay on the single session thread),
    /// so flow control wins when both are requested. `getOutputFormatParallelIfPossible` still
    /// honours the `output_format_parallel_formatting` setting and the format's own support.
    const bool use_parallel = parallel_enabled && !flow_enabled;

    WriteBufferToWebSocket out_buf(socket, &fc, &ws_write_mutex);
    OutputFormatPtr output;
    bool cancelled = false;
    bool client_gone = false;

    auto note_client_gone = [&]
    {
        client_gone = true;
        if (!cancelled)
        {
            connection.sendCancel();
            cancelled = true;
        }
    };

    /// Send a JSON control frame, tolerating a client that has already left.
    auto try_control = [&](const String & event, const String & message)
    {
        if (client_gone)
            return;
        try
        {
            sendControlEvent(event, message);
        }
        catch (...)
        {
            note_client_gone();
        }
    };

    /// Server progress packets are incremental; accumulate the reads and track
    /// the latest total estimate so each pushed event carries running totals.
    UInt64 total_read_rows = 0;
    UInt64 total_read_bytes = 0;
    UInt64 total_rows_to_read = 0;

    /// Read any pending client frame (non-blocking): flow-control grants
    /// (next/pause/resume) update the shared FlowControl; a Close/broken read means
    /// the client is gone -> cancel the query and drain.
    auto poll_client_frame = [&]
    {
        if (client_gone || !socket.poll(Poco::Timespan(0), Poco::Net::Socket::SELECT_READ))
            return;
        WebSocketFrame frame;
        try
        {
            frame = readWebSocketFrame(socket, monotonicNs() + MID_QUERY_FRAME_READ_TIMEOUT_NS);
        }
        catch (...)
        {
            frame.valid = false;
        }
        applyControlFrame(socket, frame, fc, &ws_write_mutex);
        if (fc.client_gone)
            note_client_gone();
    };

    while (true)
    {
        if (!client_gone && fc.client_gone)
            note_client_gone();
        if (!client_gone && out_buf.isBroken())
            note_client_gone();

        /// Watch the client on every iteration, not only when the connection has no
        /// data ready: both remote and local connections can continuously have
        /// packets available while a client Close is pending.
        poll_client_frame();

        /// While no server data is pending, keep watching the client socket.
        while (!connection.poll(50'000 /* microseconds */))
        {
            if (!client_gone && out_buf.isBroken())
                note_client_gone();
            poll_client_frame();
        }

        if (!client_gone && fc.client_gone)
            note_client_gone();

        Packet packet = connection.receivePacket();
        switch (packet.type)
        {
            case Protocol::Server::Data:
            {
                if (client_gone)
                    break; /// Draining after cancel; discard.
                if (!output)
                {
                    auto & factory = FormatFactory::instance();
                    const Block header = packet.block.cloneEmpty();
                    output = use_parallel ? factory.getOutputFormatParallelIfPossible(format, out_buf, header, context)
                                          : factory.getOutputFormat(format, out_buf, header, context);
                }
                if (packet.block.rows() > 0)
                {
                    /// Materialize const/sparse/low-cardinality columns before writing: an
                    /// in-process LocalConnection hands blocks straight from the pipeline (e.g.
                    /// `SELECT 1` yields a ColumnConst), which row output formats mishandle.
                    /// Remote connections already materialize over the wire, so this is a cheap
                    /// no-op there. Mirrors ClientBase::onData.
                    output->write(materializeBlock(packet.block, !output->supportsSpecialSerializationKinds()));
                    output->flush(); /// Stream this block as its own frame(s).
                    if (out_buf.isBroken())
                        note_client_gone(); /// Client left mid-stream.
                }
                break;
            }
            case Protocol::Server::Exception:
            {
                /// The result is incomplete, so discard buffered bytes rather than
                /// finalizing the format (which would append a valid suffix).
                output.reset();
                out_buf.cancel();
                const String message = packet.exception ? packet.exception->displayText() : "Unknown error from server";
                LOG_DEBUG(log, "Backend query exception: {}", message);
                try_control("error", message);
                return !client_gone;
            }
            case Protocol::Server::EndOfStream:
            {
                if (!client_gone && output)
                {
                    output->finalize();
                    out_buf.finalize();
                }
                else
                {
                    output.reset();
                    out_buf.cancel();
                }
                try_control(cancelled ? "cancelled" : "end", "");
                return !client_gone;
            }
            case Protocol::Server::Progress:
            {
                /// Mid-query push: forward running progress as a JSON text frame.
                /// Text frames do not disturb the binary result stream.
                if (!client_gone)
                {
                    const auto values = packet.progress.getValues();
                    total_read_rows += values.read_rows;
                    total_read_bytes += values.read_bytes;
                    if (values.total_rows_to_read)
                        total_rows_to_read = values.total_rows_to_read;

                    const String json = R"({"event":"progress","read_rows":)" + std::to_string(total_read_rows)
                        + R"(,"read_bytes":)" + std::to_string(total_read_bytes)
                        + R"(,"total_rows_to_read":)" + std::to_string(total_rows_to_read) + "}";
                    try
                    {
                        std::lock_guard lock(ws_write_mutex);
                        sendWebSocketText(socket, json);
                    }
                    catch (...)
                    {
                        note_client_gone();
                    }
                }
                break;
            }
            case Protocol::Server::Log:
            {
                /// Server-side log lines for this query (client opts in with
                /// `SETTINGS send_logs_level=...`). Push as a control frame.
                if (!client_gone)
                {
                    try
                    {
                        sendBlockEvent("log", packet.block);
                    }
                    catch (...)
                    {
                        note_client_gone();
                    }
                }
                break;
            }
            case Protocol::Server::ProfileEvents:
            {
                /// Periodic profile-event counters emitted during execution.
                if (!client_gone)
                {
                    try
                    {
                        sendBlockEvent("profile_events", packet.block);
                    }
                    catch (...)
                    {
                        note_client_gone();
                    }
                }
                break;
            }
            /// Not surfaced to the client yet (totals / extremes / profile info);
            /// drain and continue.
            case Protocol::Server::ProfileInfo:
            case Protocol::Server::Totals:
            case Protocol::Server::Extremes:
            case Protocol::Server::TableColumns:
            case Protocol::Server::PartUUIDs:
            case Protocol::Server::TimezoneUpdate:
                break;
            default:
                LOG_WARNING(log, "Unexpected packet {} from backend, ending session", packet.type);
                output.reset();
                out_buf.cancel();
                try_control("error", "Unexpected packet from backend");
                return false;
        }
    }
}

bool WebSocketSession::executeInsert(IServerConnection & connection, const String & query, const String & input_format)
{
    LoggerPtr log = getLogger("WebSocketSession");

    /// Format the client's streamed data is in: the command's `format` if given,
    /// else the session's `?format=`. The query text is forwarded verbatim (never
    /// parsed); the backend receives native blocks via sendData regardless.
    const String data_format = !input_format.empty() ? input_format : format;

    sendBackendQuery(connection, query, /* with_pending_data */ true);

    /// A remote server waits for external-tables data before replying with the
    /// sample block; we have none, so send an empty set to unblock that native
    /// handshake. An in-process LocalConnection has no such handshake (and does
    /// not implement sendExternalTablesData), so skip it there.
    if (connection.getConnectionType() != IServerConnection::Type::LOCAL)
    {
        ExternalTablesData external_tables_data;
        connection.sendExternalTablesData(external_tables_data);
    }

    /// The server replies with the sample block describing the target structure.
    Block sample;
    while (true)
    {
        Packet packet = connection.receivePacket();
        if (packet.type == Protocol::Server::Data)
        {
            sample = packet.block;
            break;
        }
        if (packet.type == Protocol::Server::Exception)
        {
            const String message = packet.exception ? packet.exception->displayText() : "Unknown error from server";
            sendControlEvent("error", message);
            return true;
        }
        if (packet.type == Protocol::Server::EndOfStream)
        {
            /// The server did not ask for data (e.g. the query carried its own source);
            /// nothing to stream, we are done.
            sendControlEvent("end", "");
            return true;
        }
        /// Ignore Log / Progress / TableColumns / TimezoneUpdate while waiting.
    }

    /// The client streams the INSERT data as WebSocket binary frames.
    auto data_in = std::make_unique<ReadBufferFromWebSocket>(socket);
    ReadBufferFromWebSocket * ws_in = data_in.get();

    bool aborted = false;
    try
    {
        auto source = context->getInputFormat(data_format, *data_in, sample, DEFAULT_BLOCK_SIZE);
        Pipe pipe(source);
        QueryPipeline pipeline(std::move(pipe));
        PullingPipelineExecutor executor(pipeline);

        Block block;
        while (executor.pull(block))
        {
            if (!block.empty())
                connection.sendData(block, /* name */ "", /* scalar */ false);
        }
        aborted = ws_in->wasAborted();
    }
    catch (...)
    {
        /// Error parsing the client-supplied data: abort the insert.
        const String message = getCurrentExceptionMessage(false);
        LOG_DEBUG(log, "INSERT data error: {}", message);
        connection.sendCancel();
        drainUntilEndOfStream(connection);
        sendControlEvent("error", message);
        return true;
    }

    if (aborted)
    {
        /// Client disconnected mid-insert: cancel rather than commit partial data.
        connection.sendCancel();
        drainUntilEndOfStream(connection);
        return false;
    }

    connection.sendData({}, "", false); /// Empty block signals end of data.

    while (true)
    {
        Packet packet = connection.receivePacket();
        switch (packet.type)
        {
            case Protocol::Server::EndOfStream:
                sendControlEvent("end", "");
                return true;
            case Protocol::Server::Exception:
            {
                const String message = packet.exception ? packet.exception->displayText() : "Unknown error from server";
                sendControlEvent("error", message);
                return true;
            }
            default:
                break; /// Ignore progress / log / profile events.
        }
    }
}

void WebSocketSession::run(IServerConnection & connection)
{
    LoggerPtr log = getLogger("WebSocketSession");

    /// The caller owns connection setup (and, for a remote transport, eager
    /// authentication). `connection` is expected to be usable here.
    LOG_DEBUG(log, "WebSocket session started");

    while (true)
    {
        auto message = readClientMessage();
        if (!message)
            break;

        if (message->empty())
            continue;

        try
        {
            /// Route by message kind, not by parsing SQL: a {"cmd":"insert",...}
            /// control message is a streamed data INSERT; anything else is a plain
            /// query (which covers SELECT / INSERT-SELECT / inline INSERT / DDL).
            const InsertCommand ins = parseInsertCommand(*message);
            bool keep_open = false;
            if (ins.is_insert)
            {
                /// The client explicitly declared a streamed insert; honour it as-is.
                keep_open = executeInsert(connection, ins.query, ins.format);
            }
            else if (parse_enabled)
            {
                /// Opt-in (?parse=1): parse the query to classify it, report the
                /// decision to the client, and auto-route a streamed-data INSERT
                /// without requiring the explicit control message.
                const QueryClass qc = classifyQuery(*message);
                sendQueryInfoFrame(socket, qc.verb, qc.streamed_insert ? "insert" : "query");
                keep_open = qc.streamed_insert ? executeInsert(connection, *message, qc.insert_format)
                                               : executeSelect(connection, *message);
            }
            else
            {
                keep_open = executeSelect(connection, *message);
            }
            if (!keep_open)
                break;
        }
        catch (...)
        {
            const String error_message = getCurrentExceptionMessage(false);
            LOG_DEBUG(log, "Session error: {}", error_message);
            try
            {
                sendControlEvent("error", error_message);
            }
            catch (...)
            {
                /// The socket may already be gone; nothing more we can do.
                LOG_DEBUG(log, "Failed to deliver error event to client");
            }
            break;
        }
    }

    LOG_DEBUG(log, "WebSocket session ended");
}

}
