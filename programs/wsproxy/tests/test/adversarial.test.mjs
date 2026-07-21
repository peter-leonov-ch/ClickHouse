// Adversarial WebSocket-frame tests for the clickhouse-wsproxy.
//
// These exercise the proxy's RFC 6455 hardening: client->server frames MUST be
// masked, reserved opcodes and RSV bits are protocol errors, over-large frames
// are rejected before allocation, control frames (ping) are answered with a
// pong, and — most importantly — none of the above takes down the proxy for
// other clients. We use the raw-TCP client because the native WebSocket cannot
// emit malformed frames.

import { RawClient } from "./raw.mjs";
import { backendScalar, runQuery, sleep } from "./helpers.mjs";
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

async function closeCode(c, timeoutMs = 10000) {
  const deadline = Date.now() + timeoutMs;
  for (;;) {
    const remaining = deadline - Date.now();
    if (remaining <= 0) return null;
    const frame = await Promise.race([
      c.readFrame(),
      new Promise((resolve) => setTimeout(() => resolve(null), remaining)),
    ]);
    if (frame === null || frame.closed) return null;
    if (frame.opcode === 0x8) {
      return frame.payload.length >= 2 ? frame.payload.readUInt16BE(0) : null;
    }
  }
}

describe("adversarial frames", () => {
  it(
    "rejects an unmasked client frame",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // Client->server frames must be masked; an unmasked one is a violation.
      c.sendFrame({ opcode: 0x1, payload: "SELECT 1", masked: false });
      expect(await closeCode(c)).toBe(1002);
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
      expect(await closeCode(c)).toBe(1002);
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
      expect(await closeCode(c)).toBe(1002);
      c.close();
    },
    10000,
  );

  it("closes with 1002 for an invalid close status code", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    const payload = Buffer.alloc(2);
    payload.writeUInt16BE(1005);
    c.sendFrame({ opcode: 0x8, payload });
    expect(await closeCode(c)).toBe(1002);
    c.close();
  });

  it("closes with 1007 for an invalid UTF-8 close reason", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.sendFrame({ opcode: 0x8, payload: Buffer.from([0x03, 0xe8, 0xc3, 0x28]) });
    expect(await closeCode(c)).toBe(1007);
    c.close();
  });

  it("closes with 1007 for an invalid UTF-8 text message", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.sendFrame({ opcode: 0x1, payload: Buffer.from([0xc3, 0x28]) });
    expect(await closeCode(c)).toBe(1007);
    c.close();
  });

  it.each([126, 127])("closes with 1002 for non-minimal %i-bit length encoding", async (encoding) => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.sendFrame({ opcode: 0x1, payload: "x", lengthEncoding: encoding });
    expect(await closeCode(c)).toBe(1002);
    c.close();
  });

  it("closes with 1002 when the 64-bit payload length has its high bit set", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.socket.write(Buffer.from([0x81, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]));
    expect(await closeCode(c)).toBe(1002);
    c.close();
  });

  it(
    "rejects an oversized frame before allocating",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      // Advertise 100 MiB in the header but send no payload bytes. The proxy
      // must cap on the advertised length (> 16 MiB) and never allocate it.
      c.sendFrame({ opcode: 0x1, payload: Buffer.alloc(0), advertisedLen: 100 * 1024 * 1024 });
      expect(await closeCode(c)).toBe(1009);
      c.close();
    },
    10000,
  );

  it(
    "rejects a fragmented message whose aggregate payload is oversized",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      const chunk = Buffer.alloc(8 * 1024 * 1024, 0x20);
      c.sendFrame({ opcode: 0x1, payload: chunk, fin: false });
      c.sendFrame({ opcode: 0x0, payload: chunk, fin: false });
      c.sendFrame({ opcode: 0x0, payload: "x", fin: true });
      expect(await closeCode(c, 20000)).toBe(1009);
      c.close();
    },
    30000,
  );

  it(
    "rejects a continuation frame without a fragmented message",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({ opcode: 0x0, payload: "SELECT 1" });
      expect(await closeCode(c)).toBe(1002);
      c.close();
    },
    10000,
  );

  it(
    "rejects a new data frame during a fragmented message",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({ opcode: 0x1, payload: "SELECT ", fin: false });
      c.sendFrame({ opcode: 0x1, payload: "1", fin: true });
      expect(await closeCode(c)).toBe(1002);
      c.close();
    },
    10000,
  );

  it(
    "allows a ping interleaved with a fragmented query",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({ opcode: 0x1, payload: "SELECT ", fin: false });
      c.sendFrame({ opcode: 0x9, payload: "hb" });
      c.sendFrame({ opcode: 0x0, payload: "1 AS n", fin: true });

      const binary = [];
      let sawPong = false;
      let sawEnd = false;
      for (;;) {
        const f = await c.readFrame();
        if (f.closed) break;
        if (f.opcode === 0xa) {
          sawPong = f.payload.toString() === "hb";
        } else if (f.opcode === 0x2) {
          binary.push(f.payload);
        } else if (f.opcode === 0x1) {
          const event = JSON.parse(f.payload.toString());
          if (event.event === "end") {
            sawEnd = true;
            break;
          }
        }
      }
      expect(sawPong).toBe(true);
      expect(sawEnd).toBe(true);
      expect(Buffer.concat(binary).toString()).toContain('{"n":1}');
      c.close();
    },
    10000,
  );

  it(
    "bounds a partial control frame received during a query",
    async () => {
      const marker = `PARTIAL_FRAME_${Date.now()}`;
      const countSql =
        `SELECT count() FROM system.processes WHERE query LIKE '%${marker}%' ` +
        "AND query NOT LIKE '%system.processes%'";
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow");
      c.sendFrame({
        opcode: 0x1,
        payload:
          `SELECT sleepEachRow(0.2), number FROM numbers(50) ` +
          `SETTINGS max_block_size = 1 -- ${marker}`,
      });

      let streaming = false;
      for (;;) {
        const frame = await c.readFrame();
        if (frame.closed || frame.opcode === 0x8) break;
        if (frame.opcode === 0x2) {
          streaming = true;
          break;
        }
      }
      expect(streaming).toBe(true);
      expect(await backendScalar(countSql)).not.toBe("0");

      // Send only the first header byte. Mid-query polling must not block on
      // the missing second byte for the socket's normal five-minute timeout.
      c.socket.write(Buffer.from([0x81]));
      expect(await closedOrCloseFrame(c, 5000)).toBe(true);
      c.close();

      let running = "1";
      for (let i = 0; i < 20; i++) {
        running = await backendScalar(countSql);
        if (running === "0") break;
        await sleep(250);
      }
      expect(running).toBe("0");

      const r = await runQuery("SELECT 1 AS n");
      expect(r.control.event).toBe("end");
      expect(JSON.parse(r.text.trim()).n).toBe(1);
    },
    15000,
  );

  it(
    "bounds a partial control frame while waiting for flow credit",
    async () => {
      const c = new RawClient({ port: 9010 });
      await c.handshake("/?format=JSONEachRow&flow=1");
      c.sendFrame({
        opcode: 0x1,
        payload: "SELECT number FROM numbers(100) SETTINGS max_block_size = 1",
      });

      for (;;) {
        const frame = await c.readFrame();
        if (frame.closed || frame.opcode === 0x8) throw new Error("query closed before using its initial credit");
        if (frame.opcode === 0x2) break;
      }

      c.socket.write(Buffer.from([0x81]));
      expect(await closedOrCloseFrame(c, 5000)).toBe(true);
      c.close();
    },
    10000,
  );

  it("rejects a continuation frame without a message during an insert", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.sendFrame({
      opcode: 0x1,
      payload: JSON.stringify({
        cmd: "insert",
        query: "INSERT INTO default.wsp_test FORMAT JSONEachRow",
        format: "JSONEachRow",
      }),
    });
    await sleep(50);
    c.sendFrame({ opcode: 0x0, payload: '{"a":1,"b":"x"}\n' });
    expect(await closeCode(c)).toBe(1002);
    c.close();
  });

  it("reassembles a fragmented binary message during an insert", async () => {
    const marker = `fragmented-insert-${Date.now()}`;
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow");
    c.sendFrame({
      opcode: 0x1,
      payload: JSON.stringify({
        cmd: "insert",
        query: "INSERT INTO default.wsp_test FORMAT JSONEachRow",
        format: "JSONEachRow",
      }),
    });
    const row = JSON.stringify({ a: 7, b: marker }) + "\n";
    const split = Math.floor(row.length / 2);
    c.sendFrame({ opcode: 0x2, payload: row.slice(0, split), fin: false });
    c.sendFrame({ opcode: 0x9, payload: "insert-ping" });
    c.sendFrame({ opcode: 0x0, payload: row.slice(split), fin: true });
    c.sendFrame({ opcode: 0x2, payload: Buffer.alloc(0) });

    let sawPong = false;
    let sawEnd = false;
    for (;;) {
      const frame = await c.readFrame();
      if (frame.closed || frame.opcode === 0x8) break;
      if (frame.opcode === 0xa) sawPong = frame.payload.toString() === "insert-ping";
      if (frame.opcode === 0x1 && JSON.parse(frame.payload.toString()).event === "end") {
        sawEnd = true;
        break;
      }
    }
    c.close();

    expect(sawPong).toBe(true);
    expect(sawEnd).toBe(true);
    expect(await backendScalar(`SELECT count() FROM default.wsp_test WHERE b = '${marker}'`)).toBe("1");
  });

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
