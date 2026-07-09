# WebSocket proxy on top of the native-protocol client layer

## Business rationale (the actual ROI)

The value is **not** raw performance or a cleaner API — it is **deployment velocity**.
Getting changes into the managed cloud is slow and expensive. A thin, self-contained
proxy that lives at the edge (as a sidecar next to the app) lets us move logic that
would otherwise require a server-side/cloud change out to a place we can ship quickly.

Concretely, the proxy earns its keep by:

- **Edge-side format conversion** — offload the format-conversion CPU from the managed
  cluster to the sidecar, and let apps speak whatever format is convenient (`JSONEachRow`,
  `CSV`, `Arrow`, `Parquet`, …) without a cloud-side change.
- **Mid-query push over WebSocket** — stream `Progress`, `ProfileEvents`, and `Log`
  packets to the app as they arrive, which the HTTP interface cannot stream cleanly.
- **Remote backend** — unlike the in-tree web terminal (which talks to a
  `LocalConnection`), the proxy talks to a *remote* server over the native protocol.

If (edge conversion) and (mid-query push) are not the point, ROI shrinks — at that point
the server's existing HTTP interface (`FORMAT` / `default_format`) already does
server-side conversion, so keep this justification explicit.

## Goal

**Decided form factor:** a **standalone binary** under `programs/wsproxy/`, deployed as an
edge sidecar. Preference: keep it a *separate* binary rather than wiring it into the
multi-call `clickhouse` dispatch (see build note in the plan).

**Decided session model:** **strict 1:1 sidecar** — one WS session = one native
`Connection` to a fixed backend, thread-per-session blocking IO. No pooling, no replica
load-balancing.

A new native-protocol WebSocket proxy, built **in-tree**, that:

- accepts WebSocket connections from apps,
- opens one native-protocol `Connection` per session to a remote ClickHouse server,
- maps native protocol packets ↔ WebSocket frames,
- does format conversion at the edge in both directions,
- **drops** the entire CLI/REPL surface (`ClientBase`, Replxx, `Suggest`, progress
  bars, pager, history, autocomplete).

## Design decisions

### Cut line — move it down a layer

Reuse the layer *below* the client program, not the client program itself.

- **OUT (the CLI):** `programs/client/Client.cpp`, `src/Client/ClientBase.cpp`,
  Replxx line editing, terminal progress, pager, history, `Suggest.cpp`.
- **IN (the machinery):**
  - `src/Client/Connection.{h,cpp}` — native protocol: handshake, revision negotiation,
    `sendQuery`, `sendData`, `sendCancel`, `receivePacket`, compression, TLS.
  - `src/Formats/` + `FormatFactory` — full format matrix, both directions (output
    formats for results, input formats for INSERT payloads).
  - `src/DataTypes`, `src/Columns`, `src/Core` (`Block`, `Protocol.h`, `Settings`),
    `src/IO`, `src/Compression` — the type system and codecs underneath the formats.

### Existence proof already in the tree — use it as the primary template

`src/Server/WebTerminalRequestHandler.cpp` already ships a WebSocket endpoint inside
`clickhouse-server` (the "web terminal": browser → WS → embedded client). It resolves
most open questions and corrects the earlier guesses:

- **Front door is hand-rolled RFC 6455, NOT `Poco::Net::WebSocket`.** Frame read/write,
  masking, fragment reassembly, control frames, and close codes are implemented directly
  on `response.getSocket()` (`WebTerminalRequestHandler.cpp:85-258`). Copy those framing
  helpers rather than pulling in `Poco::Net::WebSocket`.
- **Front door is an HTTP request handler, not a bespoke server.** Registered in
  `HTTPHandlerFactory.cpp:450` via `HandlingRuleHTTPHandlerFactory<WebTerminalRequestHandler>`
  on `/webterminal`. This is the canonical way to accept and upgrade a WS connection in
  this codebase.

### Template set

- **Upstream (WS front door):** `WebTerminalRequestHandler` + the `HTTPHandlerFactory`
  registration.
- **Downstream (native protocol driving):** `clickhouse-benchmark`
  (`programs/benchmark/Benchmark.cpp`) — drives `Connection` directly, multi-connection,
  with no `ClientBase`. NOTE: benchmark is a *pure client* — it has no accept socket, so
  it is only a template for the downstream half.

The proxy is essentially `WebTerminalRequestHandler` with the backend swapped: keep the
WS side, delete PTY + `ClientEmbedded` (`ClientEmbedded.h:17`, a `ClientBase` subclass) +
`LocalConnection`, and substitute a **remote `Connection` + `FormatFactory`**.

### Concurrency model — single thread, one `poll` loop per session

Follow the web terminal, not a two-thread-per-session design.
`WebTerminalRequestHandler.cpp:685-695` multiplexes `{ws socket fd, pty fd}` in a single
`poll(fds, 2, 100)` loop. For the proxy this becomes `poll({ws socket, Connection socket})`:

- One thread per session (consistent with the server's own thread-per-connection TCP model).
- Cancellation falls out for free: a frame arriving on the WS fd mid-query →
  `Connection::sendCancel` (`Connection.h:128`), then drain.
- `Connection::poll(timeout)` (`Connection.h:136`) and `setAsyncCallback`
  (`Connection.h:172`) exist precisely for this interleaving — it is how the server
  watches remote connections during distributed queries.

## Packet ↔ frame mapping (to be detailed)

Per-session loop: accept WS → open one `Connection` → on query, `sendQuery` and loop
`receivePacket`, mapping packets to WS frames:

- `Data` blocks → run through an output format → binary frames.
- `Progress` / `ProfileEvents` / `Log` → small control frames.
- `Exception` / `EndOfStream` → terminators.
- **INSERT dance:** server replies to INSERT with a header block describing the table
  structure → instantiate an *input* format for whatever the app sent
  (`JSONEachRow`, `CSV`, …) → parse into `Block`s → `sendData` natively. Mirror the
  logic in `ClientBase::processInsertQuery`.
- **Cancel:** WS close frame or cancel frame → `Connection::sendCancel` → drain.

## What we inherit for free

Complete type coverage (`LowCardinality`, `Dynamic`, `Variant`, `JSON`,
`AggregateFunction` states — everything, as long as we rebase), the full format matrix in
both directions, native LZ4/ZSTD, TLS, and protocol revision negotiation so moderate
proxy/server version skew just works.

## The bill / risks

- **Marrying the ClickHouse build:** recent Clang only, ~100 submodules, serious RAM,
  long builds, and CI has to carry it.
- **Large binary** — `FormatFactory` alone drags in Arrow/Parquet/ORC/capnproto/protobuf/
  Avro; expect a few hundred MB. Fine for a sidecar; not a lightweight artifact.
- **Rebase treadmill** — monthly upstream releases. Mitigation: keep the handler
  self-contained in its own directory with near-zero diffs to shared code, so rebases
  stay mechanical. Watch for `Connection` and `FormatFactory` API churn.

## Rejected alternative

`clickhouse-cpp` (standalone C++ client library): clean CMake, none of the monorepo pain,
but has its own partial type-system reimplementation and **zero** format machinery. Since
edge-side format conversion is the whole point, it defeats the purpose. Only interesting
if we ever need just two or three formats.

## Step 2 status — DONE (branch `wsproxy-skeleton`)

Real WebSocket endpoint implemented and validated:
- `WebSocketFrames.{h,cpp}` — reusable RFC 6455 framing (lifted/adapted from
  `WebTerminalRequestHandler`): handshake accept-key, masked read / unmasked write,
  fragmentation, control frames, size caps.
- `WsProxyHandler.{h,cpp}` — HTTP entry: non-WS → info page; WS upgrade → handshake then
  an echo loop (the placeholder the step-3 session loop replaces). No `Origin` check
  (clients are apps, not browsers).
- `WsProxy.cpp` — blocks `SIGINT`/`SIGTERM` in the main thread before the server spawns
  workers, so Poco's `waitForTerminationRequest` `sigwait` catches them → **clean exit 0
  shutdown** (verified). `BaseDaemon` re-base deferred (not needed for the signal fix).

Validated with a dependency-free raw-socket WS client (`tmp/ws_test.py`): handshake +
verified `Sec-WebSocket-Accept`, text echo, 200-byte binary echo, ping/pong, close.

Test-hygiene note: background launches detach and orphan (PPID 1) if not killed by exact
binary PID; `pgrep -f` also matches the launch shell. Always test one instance, select the
PID whose command starts with `./build`, and confirm zero processes between runs.

## Step 3 status — DONE (branch `wsproxy-skeleton`)

The echo loop is replaced by a real native-protocol bridge (`ProxySession.{h,cpp}`):

- Opens one `Connection` per WebSocket session (1:1) to a backend from env
  (`WSPROXY_BACKEND_HOST`/`PORT`/`USER`/`PASSWORD`/`DATABASE`; defaults `localhost:9000`).
- Client sends a query as a text frame; output format from the WS URL `?format=`
  (default `JSONEachRow`). Results stream back as **binary frames**, one flush per block,
  via `FormatFactory::getOutputFormat` → `output->write` → `output->flush`.
- Control frames (JSON text): `{"event":"end"}`, `{"event":"error","message":...}`,
  `{"event":"cancelled"}`.
- Mid-query cancel: a Close frame detected in the interleaved `connection.poll` /
  `socket.poll` loop triggers `Connection::sendCancel`, then drains to `EndOfStream`.

Two bugs found by running end-to-end and fixed:
1. **Wasn't actually streaming** — `WriteBufferToWebSocket` only flushed on a full 1 MB
   buffer, so nothing went out until `finalize`. Fix: `output->flush()` per block.
2. **`WriteBuffer` not finalized on all paths / `nextImpl` threw on a gone client** — gave
   a "neither finalized nor canceled" warning and teardown exceptions. Fix: `nextImpl`
   latches a `broken` flag instead of throwing; every exit path finalizes (clean) or
   cancels (error/aborted).

Validated against a real ClickHouse server (installed 25.8.1) with `tmp/ws_test3.py` and
`tmp/ws_cancel_test.py`: scalar/`numbers(5)` selects, error propagation, `?format=TSV`,
multi-query sessions, per-block streaming, and mid-query cancel confirmed via
`system.processes` (query stopped server-side). Clean SIGTERM shutdown still holds.

Not yet done (next): INSERT path (plan step 6), progress/log/profile push as control
frames, config-file/auth, `BaseDaemon` hardening.

## Step 4 status (plan step 6) — INSERT path DONE (branch `wsproxy-skeleton`)

Reverse-direction bridging in `ProxySession::executeInsert`: the app sends `INSERT INTO t
[FORMAT X]` as a text frame, then streams data as **binary frames** ending with a
zero-length binary frame (clean) — a Close mid-stream aborts (→ `sendCancel`, no partial
commit). Inline `INSERT ... VALUES (...)` is handled too (data taken from the query, format
inferred as `Values`). `executeQuery` best-effort parses with `ParserQuery` and dispatches
INSERT vs SELECT.

Flow: `sendQuery(with_pending_data=true)` → `sendExternalTablesData({})` → receive sample
block → `getInputFormat` over a `ReadBufferFromWebSocket` → `PullingPipelineExecutor` pulls
blocks → `Connection::sendData` each → `sendData({})` to finish → await `EndOfStream`.

Four bugs found by running end-to-end (all fixed):
1. **`with_pending_data` must be true** for INSERT or the server never enters the send-data
   handshake.
2. **Must send empty external-tables data** (`sendExternalTablesData({})`) after the query,
   else server waits for external tables while the proxy waits for the sample → deadlock.
3. **`ReadBuffer` has no permanent EOF latch** — `next()` re-invokes `nextImpl` after a false
   return, and the input format does a trailing read; without a `finished` latch the WS read
   buffer blocked on a frame that never arrives (this was the actual hang). Fixed with a latch.
4. Used the **synchronous `PullingPipelineExecutor`** (not the async one) — runs in the
   calling thread, no query-context/thread-group requirement for a bare global `Context`.

Validated against installed ClickHouse 25.8.1 with `tmp/ws_insert_test.py`: streamed
JSONEachRow insert, inline VALUES insert, format inference, server-side row/sum verification,
INSERT-into-missing-table error. No regressions in SELECT (`tmp/ws_test3.py`) or cancel
(`tmp/ws_cancel_test.py`); clean SIGTERM shutdown holds.

Still next: progress/log/profile push as control frames, config-file/auth, `BaseDaemon`.

## Step 5 status — progress push DONE (branch `wsproxy-skeleton`)

Mid-query push implemented in `executeSelect`: each server `Progress` packet is forwarded as
a `{"event":"progress","read_rows":N,"read_bytes":M,"total_rows_to_read":T}` text frame.
Server progress is incremental, so reads are accumulated and the latest total estimate is
tracked; running totals are pushed. Text frames don't disturb the binary result stream, so
clients just skip `progress` events when reading to the terminal `end`/`error`/`cancelled`.

Validated with `tmp/ws_progress_test.py` (a ~3s `sleepEachRow` query): 32 progress events,
monotonic `read_rows` 1→30, `total_rows_to_read=30`. No regressions (SELECT/INSERT/cancel
suites updated to skip progress frames and still green).

Still next: profile-events / log push, config-file/auth, `BaseDaemon` hardening.

## Step 6 status — profile-events / log push DONE (branch `wsproxy-skeleton`)

`executeSelect` now forwards `Log` and `ProfileEvents` packets as JSON control frames via
`sendBlockEvent`: each block is serialized to `JSONEachRow` and spliced into
`{"event":"log"|"profile_events","rows":[{...},{...}]}` (reusing the output format keeps it
correct for any columns/types). `ProfileEvents` are emitted by the server automatically; `Log`
delivery requires `send_logs_level` — which the backend sets from the *query packet* settings,
not query-text SETTINGS, so the proxy applies it from a new session param `?logs=<level>`
(threaded handler → `ProxySession` → a mutable `Settings` copy in `sendBackendQuery`).

Client protocol note: `log`/`profile_events` (like `progress`) are non-terminal text frames;
clients accumulate them until the terminal `end`/`error`/`cancelled`. The JS helper's
`collect()` now returns `{ data, text, progress, logs, profileEvents, control }`.

Validated with `tmp/.../tests/test/logs.test.mjs` (18 tests total, all green): log push via
`?logs=trace`, no logs at the default level, and profile events during a slow query.

Still next: config-file/auth, `BaseDaemon` hardening. (This completes the mid-query-push story.)

## Production hardening — test push #1 (branch `wsproxy-skeleton`)

Expanded the integration suite across four areas (type matrix, format matrix, concurrency,
resilience) — 59 tests, all green. This push found and fixed a real bug and surfaced findings:

**Bug found & fixed:** `AggregateFunction`-typed columns failed with `Unknown aggregate
function sum: while receiving packet` — the proxy called only `registerFormats()`, but
deserializing `AggregateFunction` off the native wire needs the aggregate-function registry.
Fixed by calling `registerFunctions()` + `registerAggregateFunctions()` at startup (matching
`clickhouse-client`) and linking `clickhouse_aggregate_functions`. Directly threatened the
"complete type coverage" claim.

**Finding (deferred to productionization):** UInt64 renders **unquoted** in JSONEachRow through
the proxy, whereas `clickhouse-client` quotes by default — a format-settings faithfulness gap.
Matters for JS clients (unquoted 64-bit ints lose precision on `JSON.parse`). Root: the proxy
derives `FormatSettings` from a bare `Context`; clients can't control format settings. Fix later:
apply faithful defaults and/or let clients pass settings (URL params / query settings).

**Confirmed working:** session reuse after a backend error/syntax error (reused `Connection`
survives); proxy stays healthy after a client cancels mid-query; unreachable backend →
`error` event, no hang/crash; 40 concurrent sessions + 1M-row streamed result + 100-query
session longevity. Also added `WSPROXY_PORT` (configurable listen port) to enable multi-instance
resilience tests.

### Test push #2 — adversarial / backend-failure / large INSERT (all green, 72 tests)

- **adversarial.test.mjs** (raw-TCP client `test/raw.mjs`, since native `WebSocket` only
  emits well-formed frames): unmasked frame, reserved opcode, RSV bit, and oversized-advertised
  frame are all rejected with a clean close; ping→pong; and the proxy survives malformed input
  (fresh clients still work). RFC 6455 hardening confirmed — no proxy bugs.
- **backend-failure.test.mjs** (spawns its own backend+proxy on non-default ports): killing the
  backend mid-query → the proxy delivers a clean `error`/close **promptly** (~1s, not the 20s
  natural query length) and the proxy process survives. Good resilience result.
- **large-insert.test.mjs**: 500k-row streamed JSONEachRow INSERT + 100k single-frame + 100k TSV,
  verified against server-side count/sum. Streaming INSERT holds under volume.

Test-infra learnings: `spawnBackend`/`spawnProxy` in `test/proc.mjs`; killing `clickhouse-server`
requires `lsof -ti tcp:<port>` (its watchdog fork evades signalling the spawned pid, its process
group, and `pkill -f` on the config path).

### Test push #3 — exceptions (all green, 78 tests)

**exceptions.test.mjs** (6 tests) — probed the proxy's error paths, all behave correctly (no bugs):
- Exception *after* partial results: rows 0–4 stream, then the `error` event (the `Exception`-after-`Data` path: `output.reset()`/`out_buf.cancel()`).
- Session reuse after a mid-stream exception (reused `Connection` survives).
- Malformed INSERT data → `error` **and 0 rows committed** (abort via `sendCancel`, no partial commit).
- Type error in INSERT data → clean parse error.
- ClickHouse error message/code preserved faithfully.
- Session healthy after an INSERT error (fresh insert commits).

Progress was already covered for SELECT (progress.test.mjs).

### Test push #4 — edge cases + INSERT flow (all green, 85 tests)

- **edge.test.mjs**: empty result set, `LIMIT 0` (header only), DDL (CREATE/DROP → `end`, no data),
  many columns + NULLs + arrays in one row, empty-string/unicode INSERT round-trip.
- **insert-flow.test.mjs**: closing the socket mid-INSERT aborts via `sendCancel` with **0 rows
  committed** (no partial commit), and the proxy stays healthy (a clean INSERT commits afterwards).

**INSERT-progress investigated and dropped:** implemented `written_rows` forwarding, but the native
protocol emits **no** `Progress` packets for client-data INSERTs (verified: 0 events even for 1M
rows into MergeTree, 146ms). Reverted the dormant forwarding to avoid unverified/dead code. Server
INSERT progress would only appear for `INSERT ... SELECT` (server-side), which the current
`executeInsert` streaming model doesn't handle — noted as a separate limitation:
`INSERT ... SELECT` / `INSERT ... FROM INFILE` (no client data) would make `executeInsert` wait for
WS data that never comes. Fix later (detect no-data INSERTs and skip the data-streaming phase).

Coverage still thin / future pushes: slow-loris/partial-frame timeouts, TLS, protocol-revision
skew across older server versions (need old server binaries). Productionization track (separate):
auth, TLS, config file, resource limits, graceful drain, `BaseDaemon`, CI wiring.

## Productionization — Auth DONE (credential pass-through, branch `wsproxy-skeleton`)

The proxy no longer holds fixed backend creds only from env — it resolves per-session credentials
from the WebSocket upgrade request and opens the backend `Connection` **as that user**, delegating
all authN/authZ to ClickHouse (no auth system reinvented). `WsProxyHandler::resolveCredentials`
priority: `Authorization: Basic` → `X-ClickHouse-User`/`-Key` headers → `?user=`/`?password=` URL
params → the configured `WSPROXY_BACKEND_*` defaults. A per-session `BackendParams` copy is used
(never mutating the shared default). Bad credentials surface as the backend's auth error event
(pass-through; lazy connect on first query).

Tested in `auth.test.mjs` (6 tests): default user, URL params, `Basic` header, `X-ClickHouse`
headers, and wrong-password rejection via URL params and `Basic`. Header-based creds are tested via
the raw-TCP client (native/browser `WebSocket` can't set request headers — so browsers must use URL
params, which leak in URLs/logs unless behind TLS; note for the TLS work).

Also made the integration suite **self-contained**: `globalSetup` now generates the backend config
(default + `wsp_user`) via `spawnBackend`/`spawnProxy` instead of depending on gitignored
`tmp/ch/*`. 91 tests total, all green. This is most of what CI wiring needs (just a
`clickhouse-server` binary + the built proxy).

Auth follow-ups (later): reject bad creds at handshake (eager connect → close with reason) instead
of on first query; TLS (both legs) so URL-param creds and header creds aren't sent in clear.

## Plan

1. **Skeleton.** Standalone `programs/wsproxy/` binary. **DONE — builds, links, runs-to-listen.**
   Implemented on branch `wsproxy-skeleton`: `programs/wsproxy/WsProxy.cpp` (a
   `Poco::Util::ServerApplication` that creates the global `Context`, calls
   `registerFormats`, and hosts a `DB::HTTPServer` with a stub handler on port 9010) plus
   `programs/wsproxy/CMakeLists.txt` (`clickhouse_add_executable`, no multi-call wiring).
   Compiled with zero warnings on our code; linked cleanly.

   **Measured binary size (arm64 dev build):** 424 MB with debug symbols, **306 MB
   stripped**, of which ~241 MB is actual code (`__text`). Confirms *separate ≠ smaller*:
   this is roughly the same order as the 728 MB multi-call `clickhouse` dev binary, and a
   ~100 MB target is not reachable without fighting the monorepo's coupling (trimming
   `registerFormats` would claw back tens of MB, not ~200, because `Connection` mandatorily
   pulls in `DataTypes`/`Columns`/`IO`/`Compression`/`Core`).

   **RESOLVED — clean, not a fight (build wiring).**

   **Smoke test (runtime):** launches, binds port 9010 instantly, answers HTTP `GET` with
   the stub `200` (Context + `registerFormats` + `DB::HTTPServer` all init fine at runtime).
   **Known teardown gap:** `SIGTERM` kills the process hard (exit 144), our `"Shutting
   down"` path never runs — because raw `Poco::Util::ServerApplication` does not mask
   signals across the `HTTPServer` worker threads the way `BaseDaemon` does. Fix in step 2:
   base the program on `BaseDaemon` (or block `SIGINT`/`SIGTERM` process-wide before
   starting the server).
   Precedent: `BUILD_STANDALONE_KEEPER` (`programs/keeper/CMakeLists.txt:26-44`) already
   produces a *separate* executable via `clickhouse_add_executable(clickhouse-keeper …)`
   linking real libraries — exactly the pattern we need. `clickhouse_add_executable` is a
   project macro (top-level `CMakeLists.txt:545`), so it is available to us.

   Concrete recipe for a separate binary that stays OUT of multi-call dispatch:
   - Create `programs/wsproxy/` with `WsProxy.cpp` (+ WS/session sources) and a
     `CMakeLists.txt`.
   - In that `CMakeLists.txt`, call **`clickhouse_add_executable(clickhouse-wsproxy <sources>)`**
     directly and `target_link_libraries(... dbms clickhouse_common_config
     clickhouse_functions daemon …)`. We can skip the `clickhouse_program_add` /
     `clickhouse-wsproxy-lib` indirection that keeper keeps — that indirection only exists
     to feed multi-call, which we are deliberately not joining.
   - Add a single `add_subdirectory(wsproxy)` line to `programs/CMakeLists.txt`.
   - **Do NOT** add a `mainEntryClickHouseWsproxy` to `programs/main.cpp` and **do NOT**
     add a `clickhouse_program_install(clickhouse-wsproxy …)` line. Omitting both is the
     entire mechanism that keeps the binary separate. Result: zero diff to shared dispatch
     code (good for the rebase treadmill).
   - Optionally guard behind `option(ENABLE_CLICKHOUSE_WSPROXY …)` mirroring the keeper
     flags, so CI can skip it.

   Crib global `Context` init + `registerFormats` from `Client.cpp` / `LocalServer.cpp`.

   **Caveat — separate ≠ smaller.** `Connection`, `ClientBase`, and `FormatFactory` all
   compile into the monolithic `dbms` library (`src/Client/CMakeLists.txt` only builds
   examples; the sources are globbed into `dbms`). We must link `dbms`, so the wsproxy
   binary will be roughly the same size as the full `clickhouse` binary (hundreds of MB).
   Keeper's standalone binary is small *because it avoids `dbms`* — we cannot. Separate-ness
   buys a distinct ELF and independent deployment, not a lighter artifact. Marginal build
   cost over multi-call is one extra link of `dbms` (our own sources are tiny).
2. **WS front door.** Lift the RFC 6455 framing helpers from `WebTerminalRequestHandler`
   (handshake, `readWebSocketFrame`, `sendWebSocketFrame`, close codes, masking,
   fragmentation). Reuse its Origin / auth / size-cap hardening.
3. **Backend `Connection`.** Open one native `Connection` per session (template on
   `clickhouse-benchmark`). Handshake + revision negotiation.
4. **Session loop.** Single `poll({ws fd, connection fd})` loop. Map query → `sendQuery`,
   `receivePacket` → WS frames per the mapping above.
5. **Output formats.** Route `Data` blocks through `FormatFactory::getOutputFormat` into
   binary frames; the app picks the format.
6. **INSERT path.** Header block → input format → `Block`s → `sendData`. Mirror
   `ClientBase::processInsertQuery`.
7. **Cancellation + teardown.** WS close/cancel → `Connection::sendCancel` → drain →
   clean close frame (RFC 6455 correct).
8. **Tests.** Integration test modeled on `tests/integration/test_webterminal`.

## Resolved decisions

- **Form factor:** standalone `programs/wsproxy/` binary; prefer a separate executable over
  multi-call dispatch (build feasibility to be confirmed in step 1).
- **Session model:** strict 1:1 (one WS session ↔ one native `Connection`), thread-per-session.

## Open questions

- ~~Separate-binary feasibility~~ — **RESOLVED (step 1):** clean via
  `clickhouse_add_executable`, precedent is `BUILD_STANDALONE_KEEPER`. See plan step 1.
- **Auth model:** in-band first-frame auth like the web terminal, or pass-through of the
  app's credentials to the backend `Connection`?
- **Backend target config:** fixed host/port from config/CLI (sidecar assumption), or per-session
  target from the WS handshake?
