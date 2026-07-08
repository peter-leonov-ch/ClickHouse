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
