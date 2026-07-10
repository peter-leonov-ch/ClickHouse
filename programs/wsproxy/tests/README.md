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

## Protocol under test

The client sends a SQL query as a **text** frame. SELECT results stream back as **binary**
frames encoded in the output format chosen via the WS URL `?format=` parameter. Mid-query
progress and the terminal outcome (`end` / `error` / `cancelled`) arrive as JSON **text**
frames. For INSERT, the client sends `INSERT INTO t [FORMAT X]` then streams data as binary
frames ending with a zero-length binary frame; closing the socket mid-query cancels it.

See `test/helpers.mjs` for the small client wrapper (`Session`, `runQuery`, `backendScalar`).

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
