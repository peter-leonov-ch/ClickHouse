import { describe, it, expect } from "vitest";
import { Session } from "./helpers.mjs";
import { RawClient } from "./raw.mjs";

// Collect frames until either a terminal control event or a quiet gap (no frame
// within `gapMs`, meaning the server is gated waiting for credit). Returns
// { binary, ended, cancelled }.
async function drainUntilGap(s, gapMs) {
  let binary = 0;
  let ended = false;
  for (;;) {
    const f = await s.nextFrame(gapMs);
    if (f === null) break; // gated: nothing arrived within the gap
    if (f.type === "binary") binary++;
    else if (f.type === "text") {
      const ev = JSON.parse(f.data).event;
      if (ev === "end" || ev === "error" || ev === "cancelled") {
        ended = true;
        break;
      }
      // progress / log / profile_events are not gated — ignore
    } else if (f.type === "close") {
      break;
    }
  }
  return { binary, ended };
}

describe("flow control (credit/window)", () => {
  it.each(["0", "-1", "not-a-number"])("rejects invalid initial credit %s", async (credit) => {
    const c = new RawClient({ port: 9010 });
    const { head } = await c.handshakeResponse(`/?format=JSONEachRow&flow=${credit}`);
    expect(head).toMatch(/^HTTP\/1\.1 400 /);
    expect(head).not.toContain("101 Switching Protocols");
    c.close();
  });

  it("closes the session on a nonpositive credit grant", async () => {
    const s = new Session("JSONEachRow", { flow: 1 });
    await s.ready();
    s.sendQuery("SELECT number FROM numbers(10) SETTINGS max_block_size = 1");
    expect((await drainUntilGap(s, 600)).binary).toBe(1);
    s.next(-1);
    const frame = await s.nextFrame(5000);
    expect(frame?.type).toBe("close");
    expect(frame?.code).toBe(1008);
    s.close();
  }, 10000);

  it("closes the session rather than overflowing accumulated credit", async () => {
    const s = new Session("JSONEachRow", { flow: 1 });
    await s.ready();
    s.sendQuery("SELECT number FROM numbers(10) SETTINGS max_block_size = 1");
    s.ws.send('{"cmd":"next","n":9223372036854775807}');
    s.ws.send('{"cmd":"next","n":9223372036854775807}');
    let close = null;
    for (;;) {
      const frame = await s.nextFrame(5000);
      if (frame === null || frame.type === "close") {
        close = frame?.type === "close" ? frame : null;
        break;
      }
    }
    expect(close?.code).toBe(1008);
    s.close();
  }, 10000);

  it("sends only up to the granted credit, then resumes on a next command", async () => {
    // One row per block => one binary frame per row; initial credit = 3.
    const s = new Session("JSONEachRow", { flow: 3 });
    await s.ready();
    s.sendQuery("SELECT number FROM numbers(20) SETTINGS max_block_size = 1");

    // Initial window: exactly 3 data frames, then the stream should stall.
    const phase1 = await drainUntilGap(s, 600);
    expect(phase1.ended).toBe(false);
    expect(phase1.binary).toBe(3);

    // Grant enough credit to finish; all remaining rows arrive and we reach end.
    s.next(100);
    const phase2 = await drainUntilGap(s, 3000);
    s.close();
    expect(phase2.ended).toBe(true);
    expect(phase1.binary + phase2.binary).toBe(20);
  }, 20000);

  it("a pause command halts the stream and a resume command continues it", async () => {
    const s = new Session("JSONEachRow", { flow: 2 });
    await s.ready();
    s.sendQuery("SELECT number FROM numbers(10) SETTINGS max_block_size = 1");

    // Consume the initial 2, then pause before granting more.
    const p1 = await drainUntilGap(s, 600);
    expect(p1.binary).toBe(2);
    s.pause();
    s.next(100); // credit is available, but paused must keep the stream halted

    const p2 = await drainUntilGap(s, 600);
    expect(p2.binary).toBe(0); // paused: no frames despite credit
    expect(p2.ended).toBe(false);

    // Resume: the remaining rows flow (credit already granted).
    s.resume();
    const p3 = await drainUntilGap(s, 3000);
    s.close();
    expect(p3.ended).toBe(true);
    expect(p1.binary + p2.binary + p3.binary).toBe(10);
  }, 20000);

  it("without ?flow, push mode is unbounded (no credit needed)", async () => {
    const s = new Session("JSONEachRow"); // no flow param
    await s.ready();
    s.sendQuery("SELECT number FROM numbers(50) SETTINGS max_block_size = 1");
    const r = await drainUntilGap(s, 3000);
    s.close();
    expect(r.ended).toBe(true);
    expect(r.binary).toBe(50); // all delivered without any next command
  }, 20000);
});
