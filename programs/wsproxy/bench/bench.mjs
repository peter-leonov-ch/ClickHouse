// Benchmark: fetch a query result through the wsproxy and measure wall time +
// bytes received. Usage: node bench.mjs <rows> <format> <iterations>
const ROWS = Number(process.argv[2] ?? 1_000_000);
const FORMAT = process.argv[3] ?? "JSONCompactEachRow";
const ITERS = Number(process.argv[4] ?? 5);
const URL = process.env.WSPROXY_URL ?? "ws://127.0.0.1:9010";

const QUERY = `SELECT number AS n, number*2 AS d, toString(number) AS s FROM numbers(${ROWS})`;

function once() {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(`${URL}/?format=${FORMAT}`);
    ws.binaryType = "arraybuffer";
    let bytes = 0;
    let t0 = 0;
    ws.addEventListener("open", () => {
      t0 = performance.now();
      ws.send(QUERY);
    });
    ws.addEventListener("message", (ev) => {
      if (typeof ev.data === "string") {
        const msg = JSON.parse(ev.data);
        if (msg.event === "end" || msg.event === "error" || msg.event === "cancelled") {
          const ms = performance.now() - t0;
          ws.close();
          if (msg.event === "end") resolve({ ms, bytes });
          else reject(new Error(`${msg.event}: ${msg.message ?? ""}`));
        }
      } else {
        bytes += ev.data.byteLength;
      }
    });
    ws.addEventListener("error", (e) => reject(new Error(`ws error: ${e?.message ?? e}`)));
  });
}

const results = [];
// One warm-up iteration (not counted).
await once();
for (let i = 0; i < ITERS; i++) {
  const r = await once();
  results.push(r);
  console.log(`  iter ${i + 1}: ${r.ms.toFixed(0)} ms, ${(r.bytes / 1e6).toFixed(1)} MB`);
}
const times = results.map((r) => r.ms).sort((a, b) => a - b);
const median = times[Math.floor(times.length / 2)];
const min = times[0];
const bytes = results[0].bytes;
const mbps = bytes / 1e6 / (min / 1000); // best-case throughput
const rowsPerSec = ROWS / (min / 1000);
console.log(
  `PROXY ${FORMAT} rows=${ROWS}: min=${min.toFixed(0)}ms median=${median.toFixed(0)}ms ` +
    `bytes=${(bytes / 1e6).toFixed(1)}MB throughput=${mbps.toFixed(0)}MB/s (${(rowsPerSec / 1e6).toFixed(1)}M rows/s)`,
);
