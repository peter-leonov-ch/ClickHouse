// Vitest global setup: ensure a ClickHouse backend (:9000) and the proxy (:9010)
// are running, create the shared test table, and tear down whatever we started.
//
// Reuses anything already listening (so you can run against a stack you started
// by hand); only processes we spawn here are killed on teardown.

import { spawn } from "node:child_process";
import net from "node:net";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const testDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(testDir, "../../../.."); // programs/wsproxy/tests/test -> repo root

const CLICKHOUSE_SERVER = process.env.CLICKHOUSE_SERVER ?? "/usr/local/bin/clickhouse-server";
const CH_CONFIG = process.env.CH_CONFIG ?? resolve(repoRoot, "tmp/ch/config.xml");
const WSPROXY_BIN =
  process.env.WSPROXY_BIN ?? resolve(repoRoot, "build/programs/wsproxy/clickhouse-wsproxy");

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

async function waitForPort(port, timeoutMs = 30_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await portOpen(port)) return;
    await new Promise((r) => setTimeout(r, 200));
  }
  throw new Error(`Timed out waiting for port ${port}`);
}

export default async function setup() {
  const spawned = [];

  if (!(await portOpen(BACKEND_PORT))) {
    const proc = spawn(CLICKHOUSE_SERVER, ["--config-file", CH_CONFIG], {
      stdio: "ignore",
      detached: false,
    });
    spawned.push(proc);
    await waitForPort(BACKEND_PORT);
  }

  if (!(await portOpen(PROXY_PORT))) {
    const proc = spawn(WSPROXY_BIN, [], { stdio: "ignore", detached: false });
    spawned.push(proc);
    await waitForPort(PROXY_PORT);
  }

  // Create the shared test table via the proxy (DDL goes through the SELECT path).
  const { runQuery } = await import("./helpers.mjs");
  const ddl = await runQuery(
    "CREATE TABLE IF NOT EXISTS default.wsp_test (a UInt32, b String) ENGINE = Memory",
  );
  if (ddl.control.event !== "end") {
    throw new Error(`failed to create test table: ${JSON.stringify(ddl.control)}`);
  }

  // Teardown: kill only what we started.
  return () => {
    for (const proc of spawned) {
      try {
        proc.kill("SIGTERM");
      } catch {
        /* ignore */
      }
    }
  };
}
