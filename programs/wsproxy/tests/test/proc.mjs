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
// The freshly-built multi-call binary (run as `clickhouse server`) — this is the one that
// carries the new ws_port. Point WS_SERVER_BIN elsewhere to test a different build.
const WS_SERVER_BIN = process.env.WS_SERVER_BIN ?? resolve(repoRoot, "build/programs/clickhouse");

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
export async function spawnBackend({ tcpPort, securePort }) {
  const dir = fs.mkdtempSync(join(os.tmpdir(), "wsproxy-ch-"));
  const configPath = join(dir, "config.xml");

  // Optional TLS: generate a self-signed cert and enable the secure native port.
  let secureBlock = "";
  if (securePort) {
    const crt = join(dir, "server.crt");
    const key = join(dir, "server.key");
    const gen = spawnSync("openssl", [
      "req", "-subj", "/CN=localhost", "-new", "-newkey", "rsa:2048",
      "-days", "3650", "-nodes", "-x509", "-keyout", key, "-out", crt,
    ]);
    if (gen.status !== 0) throw new Error(`openssl cert generation failed: ${gen.stderr}`);
    secureBlock = `
    <tcp_port_secure>${securePort}</tcp_port_secure>
    <openSSL><server>
        <certificateFile>${crt}</certificateFile>
        <privateKeyFile>${key}</privateKeyFile>
        <verificationMode>none</verificationMode>
        <cacheSessions>true</cacheSessions>
        <disableProtocols>sslv2,sslv3</disableProtocols>
        <preferServerCiphers>true</preferServerCiphers>
    </server></openSSL>`;
  }

  fs.writeFileSync(
    configPath,
    `<clickhouse>
    <logger><level>none</level><console>0</console>
        <log>${dir}/server.log</log><errorlog>${dir}/server.err.log</errorlog></logger>
    <tcp_port>${tcpPort}</tcp_port>${secureBlock}
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
    <users>
        <default>
            <password></password><networks><ip>::/0</ip></networks>
            <profile>default</profile><quota>default</quota>
        </default>
        <!-- Password-protected user for auth tests. -->
        <wsp_user>
            <password>wsp_pass</password><networks><ip>::/0</ip></networks>
            <profile>default</profile><quota>default</quota>
        </wsp_user>
    </users>
    <quotas><default/></quotas>
</clickhouse>`,
  );

  // clickhouse-server forks a watchdog whose child re-parents into its own
  // session, so signalling the spawned pid (or its group) leaves the real
  // server running. Kill by matching the unique temp config path in argv, which
  // reliably hits the actual server process (and the watchdog).
  const proc = spawn(CLICKHOUSE_SERVER, ["--config-file", configPath], { stdio: "ignore" });
  await waitForPort(tcpPort, 30_000);
  if (securePort) await waitForPort(securePort, 30_000);
  return {
    tcpPort,
    securePort,
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
export async function spawnProxy({
  listenPort,
  backendHost = "127.0.0.1",
  backendPort,
  secure = false,
  acceptInvalidCert = false,
  sendTimeoutSec,
  compression, // native codec for backend->proxy blocks: "lz4" | "zstd" | "none"
}) {
  const proc = spawn(WSPROXY_BIN, [], {
    stdio: "ignore",
    env: {
      ...process.env,
      WSPROXY_PORT: String(listenPort),
      WSPROXY_BACKEND_HOST: backendHost,
      WSPROXY_BACKEND_PORT: String(backendPort),
      ...(secure ? { WSPROXY_BACKEND_SECURE: "1" } : {}),
      ...(acceptInvalidCert ? { WSPROXY_BACKEND_ACCEPT_INVALID_CERT: "1" } : {}),
      ...(sendTimeoutSec ? { WSPROXY_CLIENT_SEND_TIMEOUT_SEC: String(sendTimeoutSec) } : {}),
      ...(compression ? { WSPROXY_BACKEND_COMPRESSION: compression } : {}),
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

/// Spawn the freshly-built clickhouse-server with the in-server WebSocket endpoint
/// (`ws_port`) enabled, alongside a `tcp_port` (the server requires at least one
/// query port to start). Same default + wsp_user users as spawnBackend, so the
/// auth tests work unchanged. Returns { url } pointing at the ws_port.
export async function spawnServerWithWs({ wsPort, tcpPort }) {
  const dir = fs.mkdtempSync(join(os.tmpdir(), "wsproxy-server-"));
  const configPath = join(dir, "config.xml");
  fs.writeFileSync(
    configPath,
    `<clickhouse>
    <logger><level>warning</level><console>0</console>
        <log>${dir}/server.log</log><errorlog>${dir}/server.err.log</errorlog></logger>
    <tcp_port>${tcpPort}</tcp_port>
    <ws_port>${wsPort}</ws_port>
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
    <users>
        <default>
            <password></password><networks><ip>::/0</ip></networks>
            <profile>default</profile><quota>default</quota>
            <access_management>1</access_management>
        </default>
        <wsp_user>
            <password>wsp_pass</password><networks><ip>::/0</ip></networks>
            <profile>default</profile><quota>default</quota>
        </wsp_user>
    </users>
    <quotas><default/></quotas>
</clickhouse>`,
  );

  const proc = spawn(WS_SERVER_BIN, ["server", "--config-file", configPath], { stdio: "ignore" });
  await waitForPort(wsPort, 30_000);
  return {
    url: `ws://127.0.0.1:${wsPort}`,
    dir,
    stop() {
      // Kill whatever holds the ws port (watchdog fork/re-parent shenanigans).
      for (const port of [wsPort, tcpPort]) {
        const r = spawnSync("lsof", ["-ti", `tcp:${port}`, "-sTCP:LISTEN"], { encoding: "utf8" });
        for (const pid of (r.stdout || "").split("\n").map((s) => s.trim()).filter(Boolean)) {
          try {
            process.kill(Number(pid), "SIGKILL");
          } catch {
            /* already gone */
          }
        }
      }
    },
  };
}
