// Adversarial WebSocket-frame tests for the clickhouse-wsproxy.
//
// These exercise the proxy's RFC 6455 hardening: client->server frames MUST be
// masked, reserved opcodes and RSV bits are protocol errors, over-large frames
// are rejected before allocation, control frames (ping) are answered with a
// pong, and — most importantly — none of the above takes down the proxy for
// other clients. We use the raw-TCP client because the native WebSocket cannot
// emit malformed frames.

import { RawClient } from "./raw.mjs";
import { runQuery } from "./helpers.mjs";
import { describe, it, expect } from "vitest";

// A protocol violation should make the proxy tear the connection down. The spec
// allows either an outright TCP close or a close (0x8) control frame first, so
// we accept both: read frames until we either see a close-opcode frame or the
// socket goes away.
async function closedOrCloseFrame(c, timeoutMs = 10000) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const remaining = deadline - Date.now();
    if (remaining <= 0) break;
    if (await c.waitClose(0)) return true;
    const f = await Promise.race([
      c.readFrame(),
      new Promise((r) => setTimeout(() => r(null), Math.min(remaining, 300))),
    ]);
    if (f === null) continue; // timed out waiting for a frame; loop and re-check
    if (f.closed) return true;
    if (f.opcode === 0x8) return true; // close frame
    // Anything else (e.g. a stray pong) is ignored; keep waiting for the close.
  }
  return c.closed;
}

describe("adversarial frames", () => {
  it(
    "rejects an unmasked client frame",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // Client->server frames must be masked; an unmasked one is a violation.
      c.sendFrame({ opcode: 0x1, payload: "SELECT 1", masked: false });
      expect(await closedOrCloseFrame(c)).toBe(true);
      c.close();
    },
    10000,
  );

  it(
    "rejects a reserved opcode (0x3)",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // 0x3-0x7 are reserved non-control opcodes.
      c.sendFrame({ opcode: 0x3, payload: "x" });
      expect(await closedOrCloseFrame(c)).toBe(true);
      c.close();
    },
    10000,
  );

  it(
    "rejects a frame with an RSV bit set",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // No extension was negotiated, so any RSV bit is a protocol error.
      c.sendFrame({ opcode: 0x1, payload: "SELECT 1", rsv: 1 });
      expect(await closedOrCloseFrame(c)).toBe(true);
      c.close();
    },
    10000,
  );

  it(
    "rejects an oversized frame before allocating",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // Advertise 100 MiB in the header but send no payload bytes. The proxy
      // must cap on the advertised length (> 16 MiB) and never allocate it.
      c.sendFrame({ opcode: 0x1, payload: Buffer.alloc(0), advertisedLen: 100 * 1024 * 1024 });
      expect(await closedOrCloseFrame(c)).toBe(true);
      c.close();
    },
    10000,
  );

  it(
    "answers a ping with a matching pong",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({ opcode: 0x9, payload: "hb" });
      const f = await c.readFrame();
      expect(f.opcode).toBe(0xa);
      expect(f.payload.toString()).toBe("hb");
      c.close();
    },
    10000,
  );

  it(
    "serves a valid masked query through the raw client",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({ opcode: 0x1, payload: "SELECT 1 AS n" });

      const binary = [];
      let sawEnd = false;
      for (;;) {
        const f = await c.readFrame();
        if (f.closed) break;
        if (f.opcode === 0x2) {
          // Binary frame: query result bytes.
          binary.push(f.payload);
          continue;
        }
        if (f.opcode === 0x1) {
          // Text frame: a control event.
          const msg = JSON.parse(f.payload.toString());
          // Non-terminal mid-query pushes: keep reading.
          if (msg.event === "progress" || msg.event === "log" || msg.event === "profile_events")
            continue;
          if (msg.event === "end") {
            sawEnd = true;
            break;
          }
          // Any other control event (e.g. error) is unexpected here.
          throw new Error(`unexpected control event: ${f.payload.toString()}`);
        }
        // Ignore stray control frames (ping/pong/close-with-data), keep reading.
      }

      expect(sawEnd).toBe(true);
      expect(Buffer.concat(binary).toString()).toContain('{"n":1}');
      c.close();
    },
    10000,
  );

  it(
    "does not throw on an idle connect-then-disconnect",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // Never send a query; just drop the connection.
      c.close();

      // The proxy must still serve new clients afterwards.
      const r = await runQuery("SELECT 1 AS n");
      expect(r.control.event).toBe("end");
      expect(JSON.parse(r.text.trim()).n).toBe(1);
    },
    10000,
  );

  // Runs last: after all the malformed traffic above, a fresh normal client
  // must still work, proving the proxy process survived every violation.
  it(
    "survives malformed input and keeps serving other clients",
    async () => {
      const r = await runQuery("SELECT 42 AS n");
      expect(r.control.event).toBe("end");
      expect(JSON.parse(r.text.trim()).n).toBe(42);
    },
    10000,
  );
});
