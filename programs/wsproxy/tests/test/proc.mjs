// Helper to spawn an extra proxy instance with a custom listen port and backend,
// used by resilience tests (e.g. pointing the proxy at a dead backend).

import { spawn } from "node:child_process";
import net from "node:net";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const testDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(testDir, "../../../..");
const WSPROXY_BIN =
  process.env.WSPROXY_BIN ?? resolve(repoRoot, "build/programs/wsproxy/clickhouse-wsproxy");

function portOpen(port, host = "127.0.0.1") {
  return new Promise((res) => {
    const socket = net.connect({ port, host }, () => {
      socket.destroy();
      res(true);
    });
    socket.on("error", () => res(false));
  });
}

async function waitForPort(port, timeoutMs = 15_000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    if (await portOpen(port)) return;
    await new Promise((r) => setTimeout(r, 100));
  }
  throw new Error(`Timed out waiting for proxy port ${port}`);
}

/**
 * Spawn a proxy on `listenPort` pointing at `backendHost:backendPort`.
 * Returns { url, stop } where stop() kills the process.
 */
export async function spawnProxy({ listenPort, backendHost = "127.0.0.1", backendPort }) {
  const proc = spawn(WSPROXY_BIN, [], {
    stdio: "ignore",
    env: {
      ...process.env,
      WSPROXY_PORT: String(listenPort),
      WSPROXY_BACKEND_HOST: backendHost,
      WSPROXY_BACKEND_PORT: String(backendPort),
    },
  });
  await waitForPort(listenPort);
  return {
    url: `ws://127.0.0.1:${listenPort}`,
    stop() {
      try {
        proc.kill("SIGKILL");
      } catch {
        /* already gone */
      }
    },
  };
}
