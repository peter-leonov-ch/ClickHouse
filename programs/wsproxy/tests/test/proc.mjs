// Helper to spawn an extra proxy instance with a custom listen port and backend,
// used by resilience tests (e.g. pointing the proxy at a dead backend).

import { spawn, spawnSync } from "node:child_process";
import fs from "node:fs";
import net from "node:net";
import os from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const testDir = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(testDir, "../../../..");
const WSPROXY_BIN =
  process.env.WSPROXY_BIN ?? resolve(repoRoot, "build/programs/wsproxy/clickhouse-wsproxy");
const CLICKHOUSE_SERVER = process.env.CLICKHOUSE_SERVER ?? "/usr/local/bin/clickhouse-server";

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
 * Spawn a disposable ClickHouse backend on `tcpPort` with a fresh temp data
 * dir, so a test can kill it (e.g. backend-drops-mid-query). Returns
 * { tcpPort, stop } where stop() kills the process and removes the temp dir.
 */
export async function spawnBackend({ tcpPort }) {
  const dir = fs.mkdtempSync(join(os.tmpdir(), "wsproxy-ch-"));
  const configPath = join(dir, "config.xml");
  fs.writeFileSync(
    configPath,
    `<clickhouse>
    <logger><level>none</level><console>0</console>
        <log>${dir}/server.log</log><errorlog>${dir}/server.err.log</errorlog></logger>
    <tcp_port>${tcpPort}</tcp_port>
    <listen_host>127.0.0.1</listen_host>
    <path>${dir}/data/</path>
    <tmp_path>${dir}/tmp/</tmp_path>
    <user_files_path>${dir}/user_files/</user_files_path>
    <mark_cache_size>536870912</mark_cache_size>
    <mlock_executable>false</mlock_executable>
    <users_config>users.xml</users_config>
    <default_profile>default</default_profile>
    <default_database>default</default_database>
</clickhouse>`,
  );
  fs.writeFileSync(
    join(dir, "users.xml"),
    `<clickhouse>
    <profiles><default/></profiles>
    <users><default>
        <password></password><networks><ip>::/0</ip></networks>
        <profile>default</profile><quota>default</quota>
    </default></users>
    <quotas><default/></quotas>
</clickhouse>`,
  );

  // clickhouse-server forks a watchdog whose child re-parents into its own
  // session, so signalling the spawned pid (or its group) leaves the real
  // server running. Kill by matching the unique temp config path in argv, which
  // reliably hits the actual server process (and the watchdog).
  const proc = spawn(CLICKHOUSE_SERVER, ["--config-file", configPath], { stdio: "ignore" });
  await waitForPort(tcpPort, 30_000);
  return {
    tcpPort,
    stop() {
      // Kill whatever holds the TCP port — reliably the real server process,
      // regardless of the watchdog fork/rename/re-parent shenanigans.
      const r = spawnSync("lsof", ["-ti", `tcp:${tcpPort}`, "-sTCP:LISTEN"], { encoding: "utf8" });
      for (const pid of (r.stdout || "").split("\n").map((s) => s.trim()).filter(Boolean)) {
        try {
          process.kill(Number(pid), "SIGKILL");
        } catch {
          /* already gone */
        }
      }
      spawnSync("pkill", ["-9", "-f", configPath]);
      try {
        proc.kill("SIGKILL");
      } catch {
        /* already gone */
      }
      try {
        fs.rmSync(dir, { recursive: true, force: true });
      } catch {
        /* ignore */
      }
    },
  };
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
