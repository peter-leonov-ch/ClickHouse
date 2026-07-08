#pragma once

#include <optional>

#include <Interpreters/Context_fwd.h>
#include <base/types.h>

#include <Poco/Net/StreamSocket.h>

namespace DB
{

class Connection;
class ASTInsertQuery;

/// Where the proxy forwards native-protocol queries.
struct BackendParams
{
    String host = "localhost";
    UInt16 port = 9000;
    String user = "default";
    String password;
    String database;
};

/// Drives one WebSocket session against a backend ClickHouse server over the
/// native protocol.
///
/// Owns a `Connection` for the lifetime of the session. For each query the
/// client sends as a WebSocket text frame, it runs `sendQuery` / `receivePacket`
/// and streams the result blocks back as binary frames (encoded with the chosen
/// output format), followed by a JSON control frame (`end` / `error` /
/// `cancelled`). A Close frame arriving mid-query triggers `Connection::sendCancel`.
class ProxySession
{
public:
    ProxySession(Poco::Net::StreamSocket & socket_, ContextPtr context_, BackendParams backend_, String format_);

    void run();

private:
    /// Read one complete client message (reassembling fragments, answering pings).
    /// Returns the message payload, or nullopt when the session should end
    /// (Close frame, read error, or protocol violation).
    std::optional<String> readClientMessage();

    /// Execute one query and stream its result. Returns false if the session
    /// should end afterwards (client closed mid-query). Dispatches to the
    /// INSERT or SELECT path depending on the parsed query kind.
    bool executeQuery(Connection & connection, const String & query);
    bool executeSelect(Connection & connection, const String & query);
    bool executeInsert(Connection & connection, const String & query, const ASTInsertQuery & insert);

    void sendBackendQuery(Connection & connection, const String & query, bool with_pending_data = false);
    void drainUntilEndOfStream(Connection & connection);
    void sendControlEvent(const String & event, const String & message);

    Poco::Net::StreamSocket & socket;
    ContextPtr context;
    BackendParams backend;
    String format;
};

}
