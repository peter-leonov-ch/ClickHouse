# Adding a WebSocket port to `clickhouse-server` itself

Feasibility notes for exposing a WebSocket interface **inside the server**, as a first-class port
next to `tcp_port` and `http_port` — instead of (or alongside) the standalone `wsproxy` sidecar.
**Status: analysis only — nothing here is built yet.** See `TODO.md` (repo root) for the proxy's
status and `programs/wsproxy/CLOUDFLARE.md` for the sidecar/edge deployment story.

## Verdict

**Technically moderate — mostly a port of existing code.** The server already ships the hard parts
(WebSocket framing, HTTP-upgrade, the port/handler-factory machinery), and the proxy's bridge logic
maps onto in-process execution almost unchanged. Estimate: **~1 week** for a working `ws_port` doing
SELECT + format + progress + cancel; **1-3 weeks** total for INSERT, auth, tests, config, and edge
cases — i.e. something upstreamable.

**But it is a different value proposition, and it re-incurs the cost the sidecar exists to avoid.**
See "The strategic catch" below before treating this as a cheaper version of the sidecar. It is not;
it is a complementary, longer-horizon play.

## Why it is mostly a port (verified against the code)

- **The WS machinery already ships in the server.** `src/Server/WebTerminalRequestHandler.cpp`
  implements RFC 6455 framing (`sendWebSocketFrame`, `readWebSocketFrame`), the HTTP upgrade,
  ping/pong and close, and is registered through the normal factory in
  `src/Server/HTTPHandlerFactory.cpp` (`handler_type == "webterminal"`). Framing + upgrade +
  registration are solved. (That handler bridges WS to an interactive **PTY terminal**
  via `ClientEmbeddedRunner`, which is *not* the query/format API we want — but it proves the plumbing.)
- **Adding a port is a well-trodden ~15-line pattern.** In `programs/server/Server.cpp`, every
  interface is a small `createServer(...)` block plus a `ServerType::Type` enum entry
  (`src/Server/ServerType.h`): `http_port`, `tcp_port`, `tcp_with_proxy_port`, `mysql_port`,
  `postgresql_port`, `grpc_port`, `prometheus`, `arrowflight_port`, interserver, keeper. A WS port is
  just another `HTTPServer` (WebSocket *is* an HTTP Upgrade) whose factory returns a WS handler.
- **The bridge target is free and identical to the proxy's abstraction.** Both the remote
  `Connection` (`src/Client/Connection.h`) and the in-process `LocalConnection`
  (`src/Client/LocalConnection.h`) implement the **same `IServerConnection`** interface
  (`sendQuery` / `receivePacket` / `sendData` / `sendCancel` / `Packet`) — exactly what the proxy's
  `ProxySession` already drives. So the server-side handler is `ProxySession` with the remote
  `Connection` swapped for a `LocalConnection`; the packet↔frame loop, format conversion,
  progress/log/profile push, cancel, and the INSERT handshake all carry over.

## What you reuse vs. what is new

**Reuse:** WS framing, the HTTP port + handler-factory machinery, `IServerConnection` /
`LocalConnection`, and the whole tested `ProxySession` bridge.

**New / changed:**

- A `WSQueryHandler : HTTPRequestHandler` — do the upgrade, then run the `ProxySession`-style bridge
  against a per-connection `LocalConnection` bound to the request's session.
- A small refactor: `ProxySession` currently takes a concrete `Connection &`; generalize it to
  `IServerConnection &` so it can drive either transport.
- **Auth is actually easier than the sidecar** — no credential pass-through to a remote backend;
  reuse the server's existing HTTP session/authentication like every other handler.
- Config docs + an integration test (model on `tests/integration/test_webterminal`).

## Two shapes

1. **Dedicated `ws_port`** (what "next to native and HTTP" asks for). Adds a core diff:
   - `programs/server/Server.cpp` — a `createServer(config, listen_host, "ws_port", …)` block returning
     a `ProtocolServerAdapter` wrapping an `HTTPServer` whose factory produces `WSQueryHandler`
     (model on the existing `http_port` block).
   - `src/Server/ServerType.h` — a `WS` entry in `enum Type` (+ its name mappings).
   - Server settings / config — a `ws_port` (and `ws_port_secure`) declaration, docs in the default
     `config.xml`.
   - `src/Server/HTTPHandlerFactory.cpp` — a factory that mounts `WSQueryHandler`.
   - `src/Server/WSQueryHandler.{h,cpp}` — the handler (the real work).
2. **Endpoint on the existing `http_port`** (like `/webterminal`). **Zero `Server.cpp` changes** — add
   a `handler_type == "ws"` (or a fixed `/ws` path) in `HTTPHandlerFactory.cpp` mounting
   `WSQueryHandler`. Least invasive; not a separate port, but delivers the same capability.

Recommendation: prototype as shape (2) (no core-server diff), and only promote to a dedicated
`ws_port` (shape 1) if a first-class port is wanted for ops parity with `tcp_port`/`http_port`.

## The strategic catch

Technically moderate — but putting it *in the server* re-incurs the exact cost the whole sidecar
project exists to avoid, and changes what the feature is worth:

- **It is a core diff, shipped on the managed-cloud release cycle.** Touching `Server.cpp`,
  `ServerType`, server settings and config is the rebase treadmill, and — more importantly — shipping
  it goes through the slow, expensive cloud-release path. **Deployment velocity was the entire ROI of
  the sidecar.** In-server is an upstream play measured in release cycles; the sidecar is a
  ship-this-week play.
- **The edge value evaporates.** In-server there is no separate hop: conversion runs *on the cluster*
  (the CPU we wanted to offload), and there is no compressed-native-over-WAN leg (the client talks WS
  straight to the server). The edge-offload and ZSTD-on-the-wire pitch (see `bench/README.md` and
  `CLOUDFLARE.md`) does **not** apply.

So it is a **different, complementary value proposition**: a server-side WS port gives clients the
*protocol* wins the HTTP interface cannot do cleanly — mid-query progress/logs, cancel-by-close,
bidirectional streaming, format-at-source — **without needing a sidecar at all**. That is genuinely
valuable and arguably the "right" long-term home for a WebSocket interface. The two are complementary:
iterate on the sidecar now (fast, edge-deployable, offloads CPU, compresses the WAN hop); propose the
in-server `ws_port` upstream once the WS protocol/shape is proven.

## Key references

- `programs/server/Server.cpp` — `http_port` / `tcp_port` `createServer` blocks (the port pattern).
- `src/Server/ServerType.h` — the interface `enum Type`.
- `src/Server/HTTPHandlerFactory.cpp` — handler registration; the `webterminal` type is the template.
- `src/Server/WebTerminalRequestHandler.cpp` — shipped WS framing + upgrade (bridges to a PTY, not us).
- `src/Client/LocalConnection.h` / `src/Client/Connection.h` — both `: public IServerConnection`.
- `programs/wsproxy/ProxySession.{h,cpp}` — the bridge to port from `Connection&` to `IServerConnection&`.
