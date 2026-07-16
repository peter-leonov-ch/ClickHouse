// Vitest global setup for the IN-SERVER WebSocket endpoint (ws_port), as opposed
// to the standalone proxy. Spawns the freshly-built clickhouse-server with a
// ws_port on the same PROXY_PORT the helpers use, so the existing test files run
// unchanged against `ws://127.0.0.1:9010`. Only the query/protocol tests apply
// here; proxy-specific suites (its own backend/TLS/compression) are excluded by
// vitest.server.config.mjs.

import net from "node:net";
import { spawnServerWithWs } from "./proc.mjs";

const TCP_PORT = 19000;
const WS_PORT = 9010; // matches PROXY_URL default in helpers.mjs

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

  if (!(await portOpen(WS_PORT))) {
    const server = await spawnServerWithWs({ wsPort: WS_PORT, tcpPort: TCP_PORT });
    stops.push(() => server.stop());
  }

  // Create the shared test table over the in-server WebSocket (DDL via the query path).
  const { runQuery } = await import("./helpers.mjs");
  const ddl = await runQuery(
    "CREATE TABLE IF NOT EXISTS default.wsp_test (a UInt32, b String) ENGINE = Memory",
  );
  if (ddl.control.event !== "end") {
    throw new Error(`failed to create test table: ${JSON.stringify(ddl.control)}`);
  }

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
