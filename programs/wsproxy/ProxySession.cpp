#include <ProxySession.h>
#include <WebSocketFrames.h>

#include <Client/Connection.h>

#include <Core/Defines.h>
#include <Core/Protocol.h>
#include <Core/QueryProcessingStage.h>
#include <Core/Settings.h>

#include <Interpreters/ClientInfo.h>
#include <Interpreters/Context.h>

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
    bool client_gone = false;
};

/// Apply one client control frame to the flow state (also answers pings). Used
/// both by the between-packets poll and by the credit gate, so `next`/`pause`/
/// `resume`/close are handled consistently wherever the client frame is read.
void applyControlFrame(Poco::Net::StreamSocket & socket, const WebSocketFrame & frame, FlowControl & fc)
{
    if (!frame.valid || frame.opcode == Opcode::Close)
    {
        fc.client_gone = true;
        return;
    }
    if (frame.opcode == Opcode::Ping)
    {
        sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
        return;
    }
    if (frame.opcode != Opcode::Text)
        return; /// Ignore stray binary during a SELECT.

    try
    {
        Poco::JSON::Parser parser;
        const auto obj = parser.parse(frame.payload).extract<Poco::JSON::Object::Ptr>();
        const String cmd = obj->optValue<String>("cmd", "");
        if (cmd == "next")
            fc.credit += obj->optValue<Poco::Int64>("n", 0);
        else if (cmd == "pause")
            fc.paused = true;
        else if (cmd == "resume")
            fc.paused = false;
    }
    catch (...)
    {
        /// Malformed control frame: ignore (the query stream is unaffected).
        LOG_DEBUG(getLogger("ProxySession"), "Ignoring malformed control frame: {}", getCurrentExceptionMessage(false));
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
        Poco::Net::StreamSocket & socket_, FlowControl * flow_ = nullptr, size_t size = DBMS_DEFAULT_BUFFER_SIZE)
        : BufferWithOwnMemory<WriteBuffer>(size), socket(socket_), flow(flow_)
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
                    frame = readWebSocketFrame(socket);
                }
                catch (...)
                {
                    flow->client_gone = true;
                    break;
                }
                applyControlFrame(socket, frame, *flow);
            }
            if (flow->client_gone)
            {
                broken = true;
                return;
            }
        }

        try
        {
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
    bool broken = false;
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
    bool nextImpl() override
    {
        /// `ReadBuffer::next()` may call `nextImpl` again after a false return
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
                frame = readWebSocketFrame(socket);
            }
            catch (...)
            {
                aborted = true;
                return false;
            }

            if (!frame.valid || frame.opcode == Opcode::Close)
            {
                aborted = true;
                return false;
            }
            if (frame.opcode == Opcode::Ping)
            {
                sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
                continue;
            }
            if (frame.opcode == Opcode::Text)
            {
                finished = true; /// Clean end-of-data control frame.
                return false;
            }

            /// Binary (or continuation) frame: an empty one is the clean end marker.
            if (frame.payload.empty())
            {
                finished = true;
                return false;
            }

            current_frame = std::move(frame.payload);
            BufferBase::set(current_frame.data(), current_frame.size(), 0);
            return true;
        }
    }

    Poco::Net::StreamSocket & socket;
    String current_frame;
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

}

ProxySession::ProxySession(
    Poco::Net::StreamSocket & socket_,
    ContextPtr context_,
    BackendParams backend_,
    String format_,
    String logs_level_,
    bool flow_enabled_,
    Int64 flow_initial_credit_)
    : socket(socket_)
    , context(std::move(context_))
    , backend(std::move(backend_))
    , format(std::move(format_))
    , logs_level(std::move(logs_level_))
    , flow_enabled(flow_enabled_)
    , flow_initial_credit(flow_initial_credit_)
{
}

void ProxySession::sendControlEvent(const String & event, const String & message)
{
    String json = R"({"event":")" + event + "\"";
    if (!message.empty())
        json += R"(,"message":")" + escapeJSON(message) + "\"";
    json += "}";
    sendWebSocketText(socket, json);
}

void ProxySession::sendBlockEvent(const String & event, const Block & block)
{
    if (block.rows() == 0)
        return;

    /// Serialize the block to JSONEachRow (one JSON object per row), then splice
    /// the rows into a JSON array so the whole batch is one valid control frame:
    /// {"event":"log","rows":[{...},{...}]}. Reusing the output format keeps this
    /// correct for whatever columns/types the server sends.
    WriteBufferFromOwnString buf;
    auto out = FormatFactory::instance().getOutputFormat("JSONEachRow", buf, block.cloneEmpty(), context);
    out->write(block);
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

    sendWebSocketText(socket, R"({"event":")" + event + R"(","rows":[)" + rows_array + "]}");
}

void ProxySession::sendBackendQuery(Connection & connection, const String & query, bool with_pending_data)
{
    /// Copy the settings so we can opt into server log delivery. The backend
    /// attaches its log queue based on `send_logs_level` in the query packet's
    /// settings (a query-text SETTINGS clause is too late), so it must be set here.
    Settings settings = context->getSettingsRef();
    if (!logs_level.empty())
        settings.set("send_logs_level", logs_level);

    auto timeouts = ConnectionTimeouts::getTCPTimeoutsWithoutFailover(settings);

    ClientInfo client_info;
    client_info.query_kind = ClientInfo::QueryKind::INITIAL_QUERY;

    /// `with_pending_data` must be true for INSERTs so the server enters the
    /// send-data handshake and replies with the sample/header block.
    connection.sendQuery(
        timeouts,
        query,
        /* query_parameters */ {},
        /* query_id */ "",
        QueryProcessingStage::Complete,
        &settings,
        &client_info,
        with_pending_data,
        /* external_roles */ {},
        /* process_progress_callback */ {});
}

void ProxySession::drainUntilEndOfStream(Connection & connection)
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
        LOG_DEBUG(getLogger("ProxySession"), "drain after cancel: {}", getCurrentExceptionMessage(false));
    }
}

std::optional<String> ProxySession::readClientMessage()
{
    String buffer;
    bool in_fragmented_message = false;

    while (true)
    {
        WebSocketFrame frame;
        try
        {
            frame = readWebSocketFrame(socket);
        }
        catch (...)
        {
            return std::nullopt;
        }

        if (!frame.valid || frame.protocol_error || frame.message_too_big)
            return std::nullopt;

        /// Control frames may interleave with data frames.
        if (frame.opcode >= 0x08)
        {
            if (frame.opcode == Opcode::Close)
                return std::nullopt;
            if (frame.opcode == Opcode::Ping)
                sendWebSocketFrame(socket, Opcode::Pong, frame.payload.data(), frame.payload.size());
            continue;
        }

        if (frame.opcode != Opcode::Continuation)
        {
            if (in_fragmented_message)
                return std::nullopt; /// New data frame during fragmentation.
            buffer = std::move(frame.payload);
            in_fragmented_message = !frame.fin;
        }
        else
        {
            if (!in_fragmented_message)
                return std::nullopt; /// Continuation without a start.
            buffer.append(frame.payload);
            if (frame.fin)
                in_fragmented_message = false;
        }

        if (frame.fin)
            return buffer;
    }
}

bool ProxySession::executeSelect(Connection & connection, const String & query)
{
    LoggerPtr log = getLogger("ProxySession");

    sendBackendQuery(connection, query);

    FlowControl fc;
    fc.enabled = flow_enabled;
    fc.credit = flow_initial_credit;

    WriteBufferToWebSocket out_buf(socket, &fc);
    OutputFormatPtr output;
    bool cancelled = false;
    bool client_gone = false;

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
            client_gone = true;
        }
    };

    auto note_client_gone = [&]
    {
        client_gone = true;
        if (!cancelled)
        {
            connection.sendCancel();
            cancelled = true;
        }
    };

    /// Server progress packets are incremental; accumulate the reads and track
    /// the latest total estimate so each pushed event carries running totals.
    UInt64 total_read_rows = 0;
    UInt64 total_read_bytes = 0;
    UInt64 total_rows_to_read = 0;

    while (true)
    {
        /// While no server data is pending, watch the client socket so a Close
        /// frame mid-query cancels the running query promptly.
        while (!connection.poll(50'000 /* microseconds */))
        {
            if (socket.poll(Poco::Timespan(0), Poco::Net::Socket::SELECT_READ))
            {
                WebSocketFrame frame;
                try
                {
                    frame = readWebSocketFrame(socket);
                }
                catch (...)
                {
                    frame.valid = false;
                }

                /// Apply the client frame: flow-control grants (next/pause/resume)
                /// update the shared FlowControl; a Close/broken read means the
                /// client is gone -> cancel the backend query and drain.
                applyControlFrame(socket, frame, fc);
                if (fc.client_gone)
                    note_client_gone();
            }
        }

        Packet packet = connection.receivePacket();
        switch (packet.type)
        {
            case Protocol::Server::Data:
            {
                if (client_gone)
                    break; /// Draining after cancel; discard.
                if (!output)
                    output = FormatFactory::instance().getOutputFormat(format, out_buf, packet.block.cloneEmpty(), context);
                if (packet.block.rows() > 0)
                {
                    output->write(packet.block);
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
                        sendWebSocketText(socket, json);
                    }
                    catch (...)
                    {
                        client_gone = true;
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
                        client_gone = true;
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
                        client_gone = true;
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

bool ProxySession::executeInsert(Connection & connection, const String & query, const String & input_format)
{
    LoggerPtr log = getLogger("ProxySession");

    /// Format the client's streamed data is in: the command's `format` if given,
    /// else the session's `?format=`. The query text is forwarded verbatim (never
    /// parsed); the backend receives native blocks via sendData regardless.
    const String data_format = !input_format.empty() ? input_format : format;

    sendBackendQuery(connection, query, /* with_pending_data */ true);

    /// The server waits for external-tables data before replying with the sample
    /// block; we have none, so send an empty set to unblock the handshake.
    ExternalTablesData external_tables_data;
    connection.sendExternalTablesData(external_tables_data);

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

void ProxySession::run()
{
    LoggerPtr log = getLogger("ProxySession");

    Connection connection(
        backend.host,
        backend.port,
        backend.database,
        backend.user,
        backend.password,
        /* proto_send_chunked_ */ "chunked_optional",
        /* proto_recv_chunked_ */ "chunked_optional",
        /* ssh_private_key_ */ SSHKey{},
        /* jwt_ */ "",
        /* quota_key_ */ "",
        /* cluster_ */ "",
        /* cluster_secret_ */ "",
        /* client_name_ */ "clickhouse-wsproxy",
        Protocol::Compression::Enable,
        backend.secure ? Protocol::Secure::Enable : Protocol::Secure::Disable,
        /* tls_sni_override_ */ "",
        /* bind_host_ */ "");

    /// Establish the backend connection up front so authentication (which happens
    /// during the native handshake) is reported immediately, rather than on the
    /// first query. On failure, tell the client and close.
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
            sendControlEvent("error", message);
            sendWebSocketClose(socket, /* 1008 policy violation */ 1008, "Authentication failed");
        }
        catch (...)
        {
            LOG_DEBUG(log, "Failed to deliver auth error to client (already gone)");
        }
        return;
    }

    LOG_DEBUG(log, "Proxy session started; backend {}:{}", backend.host, backend.port);

    while (true)
    {
        auto message = readClientMessage();
        if (!message)
            break;

        if (message->empty())
            continue;

        try
        {
            /// Route by message kind, never by parsing SQL: a {"cmd":"insert",...}
            /// control message is a streamed data INSERT; anything else is a plain
            /// query (which covers SELECT / INSERT-SELECT / inline INSERT / DDL).
            const InsertCommand ins = parseInsertCommand(*message);
            const bool keep_open = ins.is_insert ? executeInsert(connection, ins.query, ins.format)
                                                  : executeSelect(connection, *message);
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

    LOG_DEBUG(log, "Proxy session ended");
}

}
