// Vitest global setup: ensure a ClickHouse backend (:9000) and the proxy (:9010)
// are running, create the shared test table, and tear down whatever we started.
//
// Self-contained: the backend config (including the auth-test user) is generated
// by spawnBackend, so no external config files are needed. Anything already
// listening is reused (so you can run against a stack you started by hand); only
// processes we spawn here are stopped on teardown.

import net from "node:net";
import { spawnBackend, spawnProxy } from "./proc.mjs";

const BACKEND_PORT = 9000;
const PROXY_PORT = 9010;

function portOpen(port, host = "127.0.0.1") {
  return new Promise((resolve) => {
    const socket = net.connect({ port, host }, () => {
      socket.destroy();
      resolve(true);
    });
    socket.on("error", () => resolve(false));
  });
}

export default async function setup() {
  const stops = [];

  if (!(await portOpen(BACKEND_PORT))) {
    const backend = await spawnBackend({ tcpPort: BACKEND_PORT });
    stops.push(() => backend.stop());
  }

  if (!(await portOpen(PROXY_PORT))) {
    const proxy = await spawnProxy({ listenPort: PROXY_PORT, backendPort: BACKEND_PORT });
    stops.push(() => proxy.stop());
  }

  // Create the shared test table via the proxy (DDL goes through the SELECT path).
  const { runQuery } = await import("./helpers.mjs");
  const ddl = await runQuery(
    "CREATE TABLE IF NOT EXISTS default.wsp_test (a UInt32, b String) ENGINE = Memory",
  );
  if (ddl.control.event !== "end") {
    throw new Error(`failed to create test table: ${JSON.stringify(ddl.control)}`);
  }

  // Teardown: stop only what we started.
  return () => {
    for (const stop of stops) {
      try {
        stop();
      } catch {
        /* ignore */
      }
    }
  };
}
