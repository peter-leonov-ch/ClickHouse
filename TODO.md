# WebSocket query interface

## Selected scope

The upstream target for this branch is the in-server `ws_port` in
`clickhouse-server`. It exposes the shared WebSocket query protocol through an
in-process `LocalConnection` and keeps authentication, query execution, and format
conversion inside the server.

The standalone `clickhouse-wsproxy` binary remains in the branch as a compatible
prototype and test peer. It exercises the same `WebSocketFrames` and
`WebSocketSession` implementation through a remote `Connection`, but productionizing
or deploying that binary is not part of the current upstream scope.

Cloudflare deployment is analysis only. Nothing under that deployment design has
been built; see `programs/wsproxy/CLOUDFLARE.md`.

## Current implementation

- `ws_port` is registered as a dedicated HTTP-upgrade listener in
  `clickhouse-server`.
- `WSHandler` authenticates the upgrade request, creates a server session and a
  `LocalConnection`, then invokes `WebSocketSession::run`.
- `WebSocketSession` supports query results, streamed inserts, progress, logs,
  profile events, cancellation, output formats, optional SQL classification,
  optional parallel formatting, and credit-based flow control.
- `WebSocketFrames` implements the RFC 6455 frame layer shared by the server and
  standalone proxy.
- Browser upgrades with an `Origin` header are same-origin by default.
  `ws_allowed_origins` configures a comma-separated allowlist for `ws_port`, and
  `WSPROXY_ALLOWED_ORIGINS` provides the equivalent standalone-proxy setting.
  Requests without `Origin` remain valid for non-browser clients.
- Explicit Basic authentication fails closed when its header is malformed.
- The initial `?flow=N` credit and later credit grants must be positive, fit in
  `Int64`, and cannot overflow the accumulated credit.
- Client messages have a cumulative 16 MiB limit across fragments. Mid-query
  frames must complete within one second after the socket becomes readable.
- Client write failures cancel the active query. The standalone proxy also bounds
  stalled client writes with `WSPROXY_CLIENT_SEND_TIMEOUT_SEC`.
- Protocol violations, invalid close frames, oversized messages, and invalid
  flow-control grants use RFC-appropriate close handling.
- The Node.js/Vitest suite runs the common protocol cases against both `ws_port`
  and `clickhouse-wsproxy`. Adversarial cases cover fragmented message limits,
  partial frames, interleaved control frames, invalid credit, origin policy, and
  malformed credentials. Proxy-only cases cover the remote-backend boundary.

`ws_port` currently speaks plaintext WebSocket (`ws://`). It does not provide a
`ws_port_secure` listener. Deployments that expose the port outside a trusted
network must terminate TLS at an ingress, load balancer, or other trusted proxy.

## Protocol outline

- A text frame containing SQL runs a normal query. Results are sent as binary
  frames in the requested ClickHouse output format.
- A text control message with `{"cmd":"insert", ...}` starts a streamed insert;
  binary frames carry input data and an empty binary frame ends the input.
- Text control frames report progress, logs, profile events, query classification,
  and terminal `end`, `error`, or `cancelled` events.
- Closing the WebSocket while a query is running cancels the query.
- `?flow=N` enables frame-credit flow control. `?parallel=1` enables parallel
  output formatting when flow control is disabled.

The standalone proxy and `ws_port` intentionally use the same application
protocol. Features that depend on a remote native-protocol hop, such as backend
TLS and selecting its compression codec, apply only to `clickhouse-wsproxy`.

## Work required before upstream review

- Move the protocol coverage into repository-native ClickHouse CI. The current
  Vitest suites are useful development coverage but are not yet CI gates.
- Define general connection, session, memory, and concurrency limits for
  `ws_port` and the standalone proxy.
- Add graceful drain behavior so shutdown stops accepting new upgrades, cancels
  or completes active work according to policy, and closes sessions predictably.
- Add configuration documentation for `ws_port`, its plaintext-only transport,
  origin policy, resource limits, and TLS-termination expectations.
- Decide whether a native TLS listener is required for `ws_port`; until then,
  document and validate the supported TLS-termination topology.
- Replace the standalone proxy's development-only invalid-certificate mode with
  explicit CA-based backend verification before any production deployment.
- Decide whether the opt-in SQL classifier, parallel formatter, and application
  flow-control extension belong in the first upstream version or a follow-up.

## Deliberately out of scope

- A production Cloudflare Workers/Containers deployment.
- Production hardening of `clickhouse-wsproxy`, including `BaseDaemon`
  integration and a configuration file.
- A native TLS listener named `ws_port_secure`.
- Backend connection pooling or replica load balancing. The standalone proxy
  keeps one remote `Connection` per WebSocket session.

## Development validation

The test harness is documented in `programs/wsproxy/tests/README.md`. The primary
command for the selected scope is:

```bash
cd programs/wsproxy/tests
npm run test:server
```

The compatibility suite for the standalone prototype is:

```bash
cd programs/wsproxy/tests
npm test
```

Historical benchmark notes remain under `programs/wsproxy/bench/`; they motivate
the sidecar experiment but are not acceptance criteria for the in-server
`ws_port`.
