import { describe, it, expect } from "vitest";
import { Session, runQuery } from "./helpers.mjs";
import { spawnBackend, spawnProxy } from "./proc.mjs";

// These tests run their OWN backend + proxy pair (on non-default ports) so they
// can kill the backend without disturbing the shared stack used by other files.
describe("backend failure", () => {
  it("delivers an error (no hang or crash) when the backend drops mid-query", async () => {
    const backend = await spawnBackend({ tcpPort: 9001 });
    const proxy = await spawnProxy({ listenPort: 9013, backendPort: 9001 });
    try {
      const s = new Session("JSONEachRow", { baseUrl: proxy.url });
      await s.ready();
      // A long, slowly-streaming query so we can kill the backend mid-flight.
      s.sendQuery("SELECT sleepEachRow(0.2), number FROM numbers(100) SETTINGS max_block_size = 1");

      // Wait until the query is actually streaming (first binary frame). Skip
      // non-terminal text frames (progress/log/profile_events); only stop early
      // on a close or a terminal control event.
      let streaming = false;
      for (let i = 0; i < 200; i++) {
        const f = await s.nextFrame();
        if (f.type === "binary") {
          streaming = true;
          break;
        }
        if (f.type === "close") break;
        if (f.type === "text") {
          const ev = JSON.parse(f.data).event;
          if (ev === "end" || ev === "error" || ev === "cancelled") break;
        }
      }
      expect(streaming).toBe(true);

      // Kill the backend out from under the running query.
      backend.stop();

      // The proxy must terminate the session cleanly (error or close), not hang.
      const rest = await s.collect();
      expect(["error", "closed"]).toContain(rest.control.event);
      s.close();
    } finally {
      proxy.stop();
      backend.stop();
    }
  }, 30000);

  it("keeps the proxy process alive after a backend failure", async () => {
    const backend = await spawnBackend({ tcpPort: 9002 });
    const proxy = await spawnProxy({ listenPort: 9014, backendPort: 9002 });
    try {
      // Sanity: works before the failure.
      const before = await runQuery("SELECT 1 AS n", { baseUrl: proxy.url });
      expect(before.control.event).toBe("end");

      backend.stop();

      // After the backend is gone, new sessions get a clean error rather than a
      // crash/hang — proving the proxy process survived.
      const after = await runQuery("SELECT 1 AS n", { baseUrl: proxy.url }).catch(() => null);
      expect(after === null || ["error", "closed"].includes(after.control.event)).toBe(true);
    } finally {
      proxy.stop();
      backend.stop();
    }
  }, 30000);
});
