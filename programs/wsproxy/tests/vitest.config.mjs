import { defineConfig } from "vitest/config";

export default defineConfig({
  test: {
    // Launches the backend ClickHouse server and the proxy (see test/globalSetup.mjs).
    globalSetup: "./test/globalSetup.mjs",
    // These are integration tests against a single shared backend, so run test
    // files one at a time to avoid cross-file interference on shared tables.
    fileParallelism: false,
    testTimeout: 30_000,
    hookTimeout: 60_000,
  },
});
