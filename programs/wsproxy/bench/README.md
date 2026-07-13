# clickhouse-wsproxy benchmarks

Throughput benchmarks for fetching a query result as `JSONCompactEachRow`, comparing the
proxy against fetching the same bytes directly (native `clickhouse-client` and the HTTP
interface). Used to measure the cost of edge format conversion and to validate changes such
as parallel output formatting.

The reference query is
`SELECT number AS n, number*2 AS d, toString(number) AS s FROM numbers(N)` — deterministic,
server-generated, and forces real formatting of two `UInt64` columns and one `String`.

## Scripts

- `bench.mjs` — drive the proxy over WebSocket. Opens a fresh session per iteration (each is a
  new backend connection, so the cloud TLS handshake is *not* amortized — matching what
  `clickhouse-client` pays per run), sends the query, counts the streamed bytes until the
  terminal `end`, and reports min / median wall time plus throughput. One warm-up iteration is
  discarded.

  ```bash
  # against a proxy on ws://127.0.0.1:9010 (override with WSPROXY_URL)
  node bench.mjs <rows> <format> <iterations>
  node bench.mjs 3000000 JSONCompactEachRow 5
  ```

- `baselines.sh` — direct baselines for the same result, with byte-identical output (forces
  `output_format_json_quote_64bit_integers=0` so the proxy's unquoted `UInt64` matches
  `clickhouse-client`/HTTP, which quote 64-bit ints by default). Reports three paths: native
  transport + client-side format, HTTP server-side format (plain), and HTTP + gzip. Reads the
  cloud endpoint from the environment.

  ```bash
  CLICKHOUSE_CLOUD_HOST=... CLICKHOUSE_CLOUD_PASSWORD=... bash baselines.sh <rows> <iterations>
  ```

- `tcp_ceiling.mjs` — diagnostic: measures Node's raw TCP loopback receive ceiling, to confirm
  the JS client is not the bottleneck when attributing proxy overhead.

## Running against a backend

Start a proxy pointed at the backend, then run `bench.mjs`.

```bash
# Cloud backend (native secure), credentials from the environment:
WSPROXY_BACKEND_HOST="$CLICKHOUSE_CLOUD_HOST" WSPROXY_BACKEND_PORT=9440 \
WSPROXY_BACKEND_USER=default WSPROXY_BACKEND_PASSWORD="$CLICKHOUSE_CLOUD_PASSWORD" \
WSPROXY_BACKEND_SECURE=1 WSPROXY_PORT=9010 \
  ../../../build/programs/wsproxy/clickhouse-wsproxy &

node bench.mjs 3000000 JSONCompactEachRow 5
CLICKHOUSE_CLOUD_HOST="$CLICKHOUSE_CLOUD_HOST" CLICKHOUSE_CLOUD_PASSWORD="$CLICKHOUSE_CLOUD_PASSWORD" \
  bash baselines.sh 3000000 3
```

For an intrinsic (CPU-bound) measurement without the WAN, point the proxy at a local backend
(`WSPROXY_BACKEND_HOST=127.0.0.1 WSPROXY_BACKEND_PORT=<tcp> WSPROXY_BACKEND_SECURE=0`) and run
the same commands against `127.0.0.1`.

## Interpreting results

- **Over a WAN** the result is network-bound: the proxy pulls native lz4-compressed blocks over
  the wire and converts at the edge, so it lands on par with `clickhouse-client` and well ahead
  of plain (uncompressed) HTTP JSON.
- **On loopback** the result is CPU-bound and exposes the proxy's conversion throughput. The
  proxy formats output on a thread pool when `output_format_parallel_formatting` is enabled and
  the format supports it; compare against `baselines.sh` (which formats in parallel by default)
  and against a direct client run with `SETTINGS output_format_parallel_formatting=0` to see the
  single-threaded floor.
