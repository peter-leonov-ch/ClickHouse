#pragma once

#include <mutex>
#include <optional>

#include <Interpreters/Context_fwd.h>
#include <base/types.h>

#include <Poco/Net/StreamSocket.h>

namespace DB
{

class IServerConnection;

/// Bridges one WebSocket session to a ClickHouse query connection, in both
/// directions, over the native `IServerConnection` abstraction.
///
/// Transport-agnostic: the connection is injected into `run`, so the same bridge
/// serves the standalone edge proxy (a remote `Connection`) and an in-server
/// WebSocket endpoint (an in-process `LocalConnection`). For each query the client
/// sends as a text frame it runs `sendQuery` / `receivePacket` and streams the
/// result blocks back as binary frames (encoded with the chosen output format),
/// followed by a JSON control frame (`end` / `error` / `cancelled`). A Close frame
/// arriving mid-query triggers `IServerConnection::sendCancel`.
class WebSocketSession
{
public:
    WebSocketSession(
        Poco::Net::StreamSocket & socket_,
        ContextPtr context_,
        String format_,
        String logs_level_ = "",
        bool flow_enabled_ = false,
        Int64 flow_initial_credit_ = 0,
        bool parse_enabled_ = false,
        bool parallel_enabled_ = false,
        String compression_method_ = "");

    /// Drive the session loop over an already-connected `connection` until the
    /// client closes or errors. The caller owns connection setup/teardown and
    /// (for a remote transport) authentication.
    void run(IServerConnection & connection);

private:
    /// Read one complete client message (reassembling fragments, answering pings).
    /// Returns the message payload, or nullopt when the session should end
    /// (Close frame, read error, or protocol violation).
    std::optional<String> readClientMessage();

    /// Run a plain query and stream its result. Returns false if the session
    /// should end afterwards (client closed mid-query). SQL is never parsed:
    /// routing is driven by the client's message kind (a raw text frame is a
    /// query -> executeSelect; a {"cmd":"insert",...} control message opts into
    /// executeInsert). `with_pending_data=false` here, so SELECT / INSERT-SELECT /
    /// inline INSERT / DDL all work and none waits for client data.
    bool executeSelect(IServerConnection & connection, const String & query);
    /// Stream a client-supplied data INSERT: send the query with `with_pending_data`,
    /// parse the streamed frames with `input_format`, and `sendData` blocks.
    bool executeInsert(IServerConnection & connection, const String & query, const String & input_format);

    void sendBackendQuery(IServerConnection & connection, const String & query, bool with_pending_data = false);
    void drainUntilEndOfStream(IServerConnection & connection);
    void sendControlEvent(const String & event, const String & message);
    /// Serialize a Log / ProfileEvents block to a `{"event":...,"rows":[...]}` text frame.
    void sendBlockEvent(const String & event, const Block & block);

    Poco::Net::StreamSocket & socket;
    ContextPtr context;
    String format;
    String logs_level; /// If set, sent as `send_logs_level` so the backend pushes Log packets.
    bool flow_enabled; /// Opt-in credit/window flow control for the SELECT push direction.
    Int64 flow_initial_credit; /// Starting frame credit when flow control is enabled.
    bool parse_enabled; /// Opt-in (?parse=1): parse SQL to auto-route inserts and report the query kind.
    /// Opt-in (?parallel=1): format SELECT output on a thread pool for higher conversion
    /// throughput, at the cost of coarser (batched) result frames. Ignored when flow control is on.
    bool parallel_enabled;
    /// Native-protocol codec the backend uses for result blocks (`network_compression_method`):
    /// "lz4" / "zstd" for a remote transport, empty (or "none") to leave it unset (in-process
    /// connections have no wire, so this is a no-op there).
    String compression_method;

    /// Serializes complete WebSocket frame sends. With parallel output formatting the format's
    /// collector thread writes result frames while the session thread pushes progress/log/control
    /// frames; without this lock the two writers would interleave bytes and corrupt the stream.
    std::mutex ws_write_mutex;
};

}
