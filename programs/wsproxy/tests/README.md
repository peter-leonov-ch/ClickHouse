# clickhouse-wsproxy integration tests

Node.js + [Vitest](https://vitest.dev/) integration tests for `clickhouse-wsproxy`,
written from the perspective of a JavaScript client using the WebSocket API.

They exercise the proxy end-to-end against a real ClickHouse backend:

- `select.test.mjs` — SELECT queries, output-format switching (`JSONEachRow` / `TSV` /
  `CSV`), error propagation, large streamed results, multi-query sessions.
- `insert.test.mjs` — streamed INSERT (`JSONEachRow` / `TSV`), inline `VALUES`, format
  inference, and error handling, verified against server-side row counts.
- `progress.test.mjs` — mid-query progress push (`{"event":"progress",...}` frames).
- `cancel.test.mjs` — closing the socket mid-query cancels the running query server-side.

## Requirements

- Node.js >= 22 (uses the built-in global `WebSocket`; no `ws` dependency).
- A built proxy binary at `build/programs/wsproxy/clickhouse-wsproxy`.
- A `clickhouse-server` binary (default `/usr/local/bin/clickhouse-server`) and the
  throwaway config at `tmp/ch/config.xml` (see the project setup).

## Running

```bash
cd programs/wsproxy/tests
npm install
npm test
```

`test/globalSetup.mjs` launches the backend (`:9000`) and the proxy (`:9010`) if they are
not already listening, creates the shared `default.wsp_test` table, and tears down whatever
it started. Override binary/config locations with the `CLICKHOUSE_SERVER`, `CH_CONFIG`,
`WSPROXY_BIN`, and `WSPROXY_URL` environment variables.

### Running against the in-server `ws_port`

The same query/protocol tests also run against the **in-server** WebSocket endpoint (a
`clickhouse-server` built with a `ws_port`, see `programs/wsproxy/IN_SERVER.md`) — the bridge code
is shared, so behavior should match:

```bash
npm run test:server   # vitest run --config vitest.server.config.mjs
```

This spawns the freshly-built `build/programs/clickhouse server` with a `ws_port` on `:9010`
(override the binary with `WS_SERVER_BIN`). Proxy-only suites (a separate remote backend,
proxy→backend TLS, and the `WSPROXY_BACKEND_COMPRESSION` codec) are excluded — see the config.
Current status: proxy suite 113/113, in-server suite 102/102.

## Protocol under test

The client sends a SQL query as a **text** frame; results stream back as **binary** frames encoded
in the output format chosen via the WS URL `?format=` parameter. Mid-query progress and the terminal
outcome (`end` / `error` / `cancelled`) arrive as JSON **text** frames. This plain-query path covers
SELECT, `INSERT … SELECT`, inline `INSERT … VALUES`, and DDL.

**Opt-in parallel output formatting (`?parallel=1`).** By default the proxy formats SELECT output
on the single session thread (one WS frame per result block, fine-grained streaming). With
`?parallel=1` it formats on a thread pool (`getOutputFormatParallelIfPossible`), which on a fast link
raises conversion throughput (~1.6× for `JSONCompactEachRow` in local benchmarks) at the cost of
coarser, batched result frames. It is mutually exclusive with flow control (`?flow`), which reads the
client socket on the session thread; flow control wins when both are set. Delivered bytes are
identical either way.

**The proxy never parses SQL** — routing is by message kind. A **streamed data INSERT** (where the
client sends the rows, e.g. for edge format conversion) is opted into with a control message
`{"cmd":"insert","query":"INSERT INTO t FORMAT JSONEachRow","format":"JSONEachRow"}`, after which the
client streams the data as **binary** frames ending with a zero-length binary frame (`format` is the
input format of that data; it defaults to the session `?format=`). Closing the socket mid-query
cancels it. The `Session.insert(query, chunks, { format })` helper wraps this.

**Opt-in SQL parsing (`?parse=1`).** For clients that don't want to classify their own SQL, connect
with `?parse=1`. The proxy then parses each query to (a) push a non-terminal `{"event":"query",
"kind":"insert"|"query","verb":"<LEADING KEYWORD>"}` frame telling the client the leading verb and
the routing decision, and (b) auto-route a streamed-data INSERT (`INSERT INTO t [FORMAT X]` with no
SELECT source / INFILE / inline data) without needing the `{"cmd":"insert"}` message — the client
just sends the query text and then streams the binary data frames. `kind` is `insert` when the proxy
will read client data, else `query`. Parsing is **off by default**: the proxy does not parse SQL
unless the client explicitly asks it to, and a parse failure degrades to the plain-query path (the
backend reports any real error). This is the only place the proxy parses SQL.

See `test/helpers.mjs` for the small client wrapper (`Session`, `runQuery`, `backendScalar`).

For deploying the proxy on Cloudflare (Containers fronted by stateless Workers), see
[`../CLOUDFLARE.md`](../CLOUDFLARE.md).

## Receive backpressure (important for large results)

The proxy pushes result frames as fast as the connection allows. A plain **event-based
`WebSocket`** (browser or Node) has **no receive-backpressure API** — it eagerly drains the socket
and fires `onmessage` regardless of whether your app has kept up. So streaming a **large** result
to a **slow consumer** accumulates in the JS heap and can OOM the client. The proxy cannot prevent
this (from its side the socket looks healthy). If you stream large results, apply backpressure. The proxy offers a built-in, portable option,
plus client-side alternatives:

- **Built-in — opt-in credit flow control (`?flow=N`).** Connect with `?flow=N` to start with N
  frames of credit; the proxy then sends a frame only while you have credit and aren't paused, and
  blocks (throttling the backend) otherwise. Grant more as you consume, or pause/resume:

  ```js
  const s = new Session("JSONEachRow", { flow: 8 }); // start with 8 frames of credit
  await s.ready();
  s.sendQuery("SELECT ... FROM big_table");
  for (;;) {
    const f = await s.nextFrame();
    if (f.type === "binary") { await process(f.data); s.next(1); }        // 1 credit per consumed frame
    else if (f.type === "text") { const e = JSON.parse(f.data); if (["end","error","cancelled"].includes(e.event)) break; }
  }
  // s.pause() / s.resume() give a coarse hard stop as well.
  ```

  A frame is one WS binary message (one result block), so credit is in frames — no byte accounting.
  Works in any JS runtime (browser + Node). INSERT direction is unaffected (client-driven).

  **Prefer throttling over an indefinite `pause()`.** When you grant small `next()` credits as you
  consume, control frames (progress / logs / errors and the terminal `end`) keep flowing between
  data frames. A hard `pause()` (or letting credit sit at 0) also halts control delivery: a
  fully-paused client will not see progress, errors, or `end` until it grants credit or resumes.
  This is inherent — once the proxy stops reading, the backend stalls, and its stream is ordered
  (control is interleaved behind data), so control cannot "jump ahead" of paused data. Use `pause()`
  as a short-term stop, not a long-lived one.

The client-side transport options also work if you prefer them:

- **Best — `WebSocketStream`** (Chromium; Node with `--experimental-websocket-stream`). Its
  `ReadableStream` applies real backpressure: when your reader is slow it stops reading the socket,
  the TCP window closes, and the proxy throttles the backend. No app protocol needed.

  ```js
  const wss = new WebSocketStream("ws://localhost:9010/?format=JSONEachRow");
  const { readable, writable } = await wss.opened;
  await writable.getWriter().write("SELECT ... FROM big_table");
  const reader = readable.getReader();
  for (;;) {
    const { value, done } = await reader.read(); // slow processing here throttles the proxy
    if (done) break;
    await process(value); // string = control/progress; Uint8Array = result bytes
  }
  ```

- **Node with the `ws` library** — pause the underlying socket by consumption:

  ```js
  ws.on("message", (data, isBinary) => {
    ws._socket.pause();                     // stop reading -> TCP backpressure to the proxy
    process(data).finally(() => ws._socket.resume());
  });
  ```

- **Plain browser `WebSocket`** (Firefox/Safari, or not using `WebSocketStream`) — you cannot apply
  receive backpressure. Keep results bounded (`LIMIT`, server-side aggregation), or consume
  synchronously in `onmessage` without queuing. Don't stream unbounded results to a slow consumer.

(If this becomes a common need, the proxy could add opt-in credit/byte-window flow control —
`sent − acked ≥ window` pauses the stream — which works for any client. Not implemented yet.)
