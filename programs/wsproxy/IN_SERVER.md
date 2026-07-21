# In-server WebSocket port

The primary upstream scope of this branch is a dedicated `ws_port` in
`clickhouse-server`. The standalone `clickhouse-wsproxy` binary remains a
compatible prototype that drives the same WebSocket session implementation over a
remote native-protocol `Connection`.

## Status

The prototype is implemented. `Server::main` creates an HTTP server for
`ws_port`, `HTTPHandlerFactory` routes every request on that listener to
`WSHandler`, and the handler runs a `WebSocketSession` over an in-process
`LocalConnection`.

The shared Node.js/Vitest protocol suite can run against either implementation.
The in-server configuration excludes only tests that inherently require a
separate proxy or remote backend, such as proxy-to-backend TLS and backend codec
selection. See `tests/README.md` for the current commands and prerequisites.

This remains a prototype rather than a production-ready interface. The branch now
includes origin and credential enforcement, bounded fragmented messages and
partial-frame reads, safe flow-control accounting, cancellation on client write
failure, RFC close handling, and adversarial tests. Remaining TLS, general
resource-limit, graceful-drain, and ClickHouse CI work is tracked in the
repository-level `TODO.md`.

## Architecture

`ws_port` is an HTTP listener because WebSocket connections begin with an HTTP
Upgrade request. Its request path is:

```text
client
  -> ws_port
  -> WSHandler
  -> authenticated Session
  -> LocalConnection
  -> WebSocketSession
  -> query pipeline
```

The shared bridge keeps framing and application-protocol behavior aligned between
the two executables:

- `WebSocketFrames` reads and writes RFC 6455 frames.
- `WebSocketSession::run` maps WebSocket messages to `IServerConnection` packets
  and formats query results.
- `WSHandler` supplies an authenticated `LocalConnection`.
- `WsProxyHandler` supplies a remote `Connection` in the standalone prototype.

The in-server path does not pass credentials to another service. It authenticates
the upgrade request through the server session and executes under that session's
user and settings.

Browser requests that include an `Origin` header must be same-origin by default.
Set `ws_allowed_origins` to a comma-separated list of allowed HTTP or HTTPS origins
when clients are intentionally hosted elsewhere. Origins are normalized before
comparison. A missing `Origin` is accepted so non-browser WebSocket clients can
connect.

## Transport security

`ws_port` currently supports plaintext WebSocket (`ws://`) only. There is no
`ws_port_secure` implementation in this branch. Exposing the listener beyond a
trusted network therefore requires TLS termination at a trusted ingress, load
balancer, or reverse proxy, with the origin and forwarded-request policy configured
explicitly.

The standalone proxy's backend TLS options protect a different connection: the
native-protocol hop from `clickhouse-wsproxy` to a remote ClickHouse server. They do
not add TLS to `ws_port`.

## Differences between local and remote connections

The bridge targets `IServerConnection`, but `LocalConnection` has several relevant
behavioral differences from a remote `Connection`:

1. Local blocks must be materialized before row-format output. A local pipeline can
   return a `ColumnConst`, while blocks decoded from the native protocol are already
   materialized.
2. The authenticated user must come from the server session. Supplying a fabricated
   `ClientInfo` to `LocalConnection::sendQuery` can replace the query context's user.
3. `LocalConnection` does not need the native protocol's external-table handshake;
   `LocalConnection::sendExternalTablesData` is not implemented.
4. `LocalConnection::poll` yields after an executor timeout so the bridge can
   inspect the WebSocket between packets. `LocalConnection::receivePacket` retains
   blocking semantics for direct consumers.
5. Query settings used by `LocalConnection` must be applied to its context. For
   example, `WSHandler` applies `send_logs_level` to the authenticated session
   context.

These are implementation constraints of the shared bridge, not alternate protocol
behaviors for clients.

## Product boundary

The in-server interface provides bidirectional streaming, cancellation by close,
and mid-query progress, log, and profile-event delivery without another deployed
service. Format conversion happens on the ClickHouse server.

The standalone sidecar explores a different deployment tradeoff: it can perform
format conversion away from the server and use a compressed native-protocol hop to
a remote backend. That experiment is compatible with the in-server protocol but is
not the selected upstream deliverable.

The Cloudflare Workers/Containers design in `CLOUDFLARE.md` is unbuilt analysis for
the sidecar and is not part of `ws_port`.

## Key files

- `programs/server/Server.cpp` registers the `ws_port` listener.
- `src/Server/ServerType.h` defines the `WS` server type.
- `src/Server/HTTPHandlerFactory.cpp` creates the `WSHandler` factory.
- `src/Server/WSHandler.{h,cpp}` authenticates and upgrades requests.
- `src/Server/WebSocketFrames.{h,cpp}` implements WebSocket framing.
- `src/Server/WebSocketSession.{h,cpp}` implements the shared query protocol.
- `src/Client/LocalConnection.h` provides in-process execution.
- `programs/wsproxy/WsProxyHandler.{h,cpp}` adapts the same session to a remote
  backend for the standalone prototype.
