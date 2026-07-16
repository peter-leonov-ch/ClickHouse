import { defineConfig } from "vitest/config";

// Runs the query/protocol test suite against the IN-SERVER WebSocket endpoint
// (clickhouse-server with a ws_port) instead of the standalone proxy. The bridge
// code is shared, so the same tests should pass. Proxy-specific suites are
// excluded: they spawn their own proxy and exercise features that only exist in
// the sidecar (a separate remote backend, proxy->backend TLS, and the
// WSPROXY_BACKEND_COMPRESSION codec).
//
// Run with:  npx vitest run --config vitest.server.config.mjs
export default defineConfig({
  test: {
    globalSetup: "./test/serverSetup.mjs",
    fileParallelism: false,
    testTimeout: 30_000,
    hookTimeout: 60_000,
    include: ["test/**/*.test.mjs"],
    exclude: [
      "test/backend-failure.test.mjs", // kills a separate backend
      "test/resilience.test.mjs", // points a proxy at a dead backend
      "test/tls.test.mjs", // proxy->backend TLS (N/A in-server)
      "test/backpressure.test.mjs", // spawns its own proxy with a send timeout
      "test/compression.test.mjs", // WSPROXY_BACKEND_COMPRESSION (proxy-only)
    ],
  },
});
