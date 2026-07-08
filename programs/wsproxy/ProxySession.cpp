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

#include <IO/BufferWithOwnMemory.h>
#include <IO/ConnectionTimeouts.h>
#include <IO/WriteBuffer.h>

#include <Common/Exception.h>
#include <Common/SSHWrapper.h>
#include <Common/logger_useful.h>


namespace DB
{

using namespace DB::WsProxy;

namespace
{

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
    explicit WriteBufferToWebSocket(Poco::Net::StreamSocket & socket_, size_t size = DBMS_DEFAULT_BUFFER_SIZE)
        : BufferWithOwnMemory<WriteBuffer>(size), socket(socket_)
    {
    }

    ~WriteBufferToWebSocket() override = default;

    bool isBroken() const { return broken; }

private:
    void nextImpl() override
    {
        if (broken || offset() == 0)
            return;
        try
        {
            sendWebSocketBinary(socket, working_buffer.begin(), offset());
        }
        catch (...)
        {
            broken = true;
        }
    }

    Poco::Net::StreamSocket & socket;
    bool broken = false;
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
                    out += "\\u00";
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

ProxySession::ProxySession(Poco::Net::StreamSocket & socket_, ContextPtr context_, BackendParams backend_, String format_)
    : socket(socket_), context(std::move(context_)), backend(std::move(backend_)), format(std::move(format_))
{
}

void ProxySession::sendControlEvent(const String & event, const String & message)
{
    String json = "{\"event\":\"" + event + "\"";
    if (!message.empty())
        json += ",\"message\":\"" + escapeJSON(message) + "\"";
    json += "}";
    sendWebSocketText(socket, json);
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

bool ProxySession::executeQuery(Connection & connection, const String & query)
{
    LoggerPtr log = getLogger("ProxySession");

    const auto & settings = context->getSettingsRef();
    auto timeouts = ConnectionTimeouts::getTCPTimeoutsWithoutFailover(settings);

    ClientInfo client_info;
    client_info.query_kind = ClientInfo::QueryKind::INITIAL_QUERY;

    connection.sendQuery(
        timeouts,
        query,
        /* query_parameters */ {},
        /* query_id */ "",
        QueryProcessingStage::Complete,
        &settings,
        &client_info,
        /* with_pending_data */ false,
        /* external_roles */ {},
        /* process_progress_callback */ {});

    WriteBufferToWebSocket out_buf(socket);
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

                /// A Close frame (or broken read) means the client is gone:
                /// cancel the backend query and drain until EndOfStream. Other
                /// in-query frames are ignored for now.
                if (!frame.valid || frame.opcode == Opcode::Close)
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
            /// Not surfaced to the client yet (progress/log/profile push is a
            /// later refinement); drain and continue.
            case Protocol::Server::Progress:
            case Protocol::Server::ProfileInfo:
            case Protocol::Server::ProfileEvents:
            case Protocol::Server::Totals:
            case Protocol::Server::Extremes:
            case Protocol::Server::Log:
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
        Protocol::Secure::Disable,
        /* tls_sni_override_ */ "",
        /* bind_host_ */ "");

    LOG_DEBUG(log, "Proxy session started; backend {}:{}", backend.host, backend.port);

    while (true)
    {
        auto query = readClientMessage();
        if (!query)
            break;

        if (query->empty())
            continue;

        try
        {
            if (!executeQuery(connection, *query))
                break;
        }
        catch (...)
        {
            const String message = getCurrentExceptionMessage(false);
            LOG_DEBUG(log, "Session error: {}", message);
            try
            {
                sendControlEvent("error", message);
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
