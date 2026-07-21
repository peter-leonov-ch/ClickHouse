import { describe, it, expect } from "vitest";
import { runQuery } from "./helpers.mjs";
import { RawClient } from "./raw.mjs";
import { spawnProxy } from "./proc.mjs";

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

describe("backpressure / stalled client", () => {
  it("drops a client that stops reading (send timeout) and stays healthy", async () => {
    // Short send timeout so the test doesn't wait the 30s default.
    const proxy = await spawnProxy({ listenPort: 9017, backendPort: 19000, sendTimeoutSec: 1 });
    try {
      const c = new RawClient({ port: 9017 });
      await c.handshake("/?format=JSONEachRow");
      // A large result the client will refuse to read, so the proxy's blocking
      // writes fill the socket buffers and then stall.
      c.sendFrame({
        opcode: 0x1,
        payload: "SELECT number, repeat('x', 1000) AS s FROM numbers(5000000)",
      });
      c.pause(); // stop reading -> proxy writes block -> ~1s send timeout should fire

      // Wait past the send timeout, then resume and confirm the proxy dropped us
      // (rather than blocking a handler thread forever).
      await sleep(4000);
      c.resume();
      const start = Date.now();
      let closed = false;
      while (Date.now() - start < 10000) {
        const f = await c.readFrame();
        if (f.closed) {
          closed = true;
          break;
        }
      }
      c.close();
      expect(closed).toBe(true);

      // The proxy is still healthy for a fresh client (the stalled thread was released).
      const ok = await runQuery("SELECT 1 AS n", { baseUrl: proxy.url });
      expect(ok.control.event).toBe("end");
      expect(JSON.parse(ok.text.trim()).n).toBe(1);
    } finally {
      proxy.stop();
    }
  }, 30000);
});
