# WebSocket integration tests

This directory contains Node.js/Vitest integration tests for the shared WebSocket
query protocol. The primary target is the in-server `ws_port`; the same common
tests also validate the compatible `clickhouse-wsproxy` prototype.

The suites cover query and format handling, streamed inserts, progress, logs,
profile events, cancellation, authentication, flow control, malformed and partial
frames, fragmented-message limits, interleaved control frames, origin policy,
concurrency, and session reuse. Tests that require a remote backend are proxy-only.

## Requirements

- Node.js 22 or newer.
- Dependencies installed with `npm install` in this directory.
- For `ws_port`, a built server at `build/programs/clickhouse` or a path supplied
  through `WS_SERVER_BIN`.
- For the standalone suite, a built proxy at
  `build/programs/wsproxy/clickhouse-wsproxy` and a `clickhouse-server` at
  `/usr/local/bin/clickhouse-server`, or paths supplied through `WSPROXY_BIN` and
  `CLICKHOUSE_SERVER`.

The harness generates temporary ClickHouse configuration under its own temporary
directory, starts the required processes, and stops the processes it started. No
pre-existing ClickHouse configuration is required.

## Run the primary in-server suite

```bash
cd programs/wsproxy/tests
npm install
npm run test:server
```

`vitest.server.config.mjs` starts `clickhouse-server` with both `ws_port` and
`tcp_port`. It excludes cases that require the standalone proxy boundary, including
proxy-to-backend TLS, remote-backend failure, backend compression selection, and
proxy-specific send-timeout configuration.

Origin-policy tests configure an allowlist and verify accepted and rejected browser
origins, plus clients without an `Origin` header. The implementation's empty-list
behavior is same-origin only.

To select another server binary:

```bash
WS_SERVER_BIN=/path/to/clickhouse npm run test:server
```

## Run the standalone compatibility suite

```bash
cd programs/wsproxy/tests
npm install
npm test
```

The default configuration starts a ClickHouse backend and
`clickhouse-wsproxy`. Override binary and endpoint selection with
`CLICKHOUSE_SERVER`, `WSPROXY_BIN`, and `WSPROXY_URL` when needed.

The proxy applies the same browser policy through `WSPROXY_ALLOWED_ORIGINS`: an
`Origin` header must be same-origin unless it appears in that comma-separated
allowlist, while requests without `Origin` are accepted.

## Protocol exercised by the tests

A client sends SQL in a text frame. Query data is returned in binary frames using
the output format selected by `?format=`. Progress, logs, profile events, query
classification, and terminal outcomes are JSON text frames.

A streamed insert begins with a control message:

```json
{"cmd":"insert","query":"INSERT INTO t FORMAT JSONEachRow","format":"JSONEachRow"}
```

The client then sends input data in binary frames and finishes with an empty binary
frame. Closing the WebSocket while a query or insert is active requests
cancellation.

Optional protocol modes include:

- `?flow=N` starts with `N` frames of output credit. A client grants more credit as
  it consumes frames. A client at zero credit also delays ordered control events,
  so applications should grant small increments instead of pausing indefinitely.
- `?parallel=1` requests parallel output formatting. It is disabled when flow
  control is active.
- `?parse=1` asks the server to classify SQL and report its leading verb and route.
  Classification failures return an error instead of silently selecting another
  route. Explicit insert control messages remain the unambiguous streamed-insert
  path.

`test/helpers.mjs` contains the `Session`, `runQuery`, and `backendScalar` test
helpers. `test/raw.mjs` provides raw frame construction for adversarial cases.

## Backpressure

Browser-style event-based `WebSocket` APIs eagerly receive messages and do not
provide receive backpressure. Tests and applications that process large results
can use the `?flow=N` extension, a `WebSocketStream` reader where available, or a
transport that can pause socket reads. Plain browser clients should keep results
bounded when they cannot consume them promptly.

The in-server `ws_port` currently uses plaintext `ws://`. TLS termination is an
external deployment responsibility; no `ws_port_secure` listener is built in this
branch.

Cloudflare deployment described in `../CLOUDFLARE.md` is unbuilt analysis for the
standalone proxy and is not a test target.
