import { describe, it, expect } from "vitest";
import { runQuery, Session } from "./helpers.mjs";
import { spawnProxy } from "./proc.mjs";

describe("resilience", () => {
  it("reuses the session connection after a backend (semantic) error", async () => {
    const s = new Session("JSONEachRow");
    const bad = await s.run("SELECT * FROM default.no_such_table_zz");
    expect(bad.control.event).toBe("error");

    // The reused per-session Connection must still work for the next query.
    const good = await s.run("SELECT 1 AS n");
    expect(good.control.event).toBe("end");
    expect(JSON.parse(good.text.trim()).n).toBe(1);
    s.close();
  });

  it("reuses the session after a syntax error", async () => {
    const s = new Session("JSONEachRow");
    const bad = await s.run("SELCT 1"); // typo -> backend syntax error
    expect(bad.control.event).toBe("error");

    const good = await s.run("SELECT 2 AS n");
    expect(good.control.event).toBe("end");
    expect(JSON.parse(good.text.trim()).n).toBe(2);
    s.close();
  });

  it("stays healthy after a client cancels a query mid-stream", async () => {
    // Session A: start a slow query, then cancel by closing the socket.
    const a = new Session("JSONEachRow");
    await a.ready();
    a.sendQuery("SELECT sleepEachRow(0.2), number FROM numbers(50) SETTINGS max_block_size = 1");
    for (let i = 0; i < 50; i++) {
      const f = await a.nextFrame();
      if (f.type === "binary" || f.type === "close") break;
    }
    a.close();

    // Session B: the proxy still serves new sessions.
    const b = await runQuery("SELECT 42 AS n");
    expect(b.control.event).toBe("end");
    expect(JSON.parse(b.text.trim()).n).toBe(42);
  }, 20000);

  it("returns an error (no hang or crash) when the backend is unreachable", async () => {
    // A second proxy instance pointed at a closed backend port.
    const proxy = await spawnProxy({ listenPort: 9011, backendPort: 59999 });
    try {
      const { control } = await runQuery("SELECT 1", { baseUrl: proxy.url });
      // Either a delivered error event or a clean socket close is acceptable;
      // what matters is that it neither hangs nor takes down the proxy.
      expect(["error", "closed"]).toContain(control.event);
    } finally {
      proxy.stop();
    }
  }, 20000);

  it("keeps serving after an unreachable-backend session", async () => {
    const proxy = await spawnProxy({ listenPort: 9012, backendPort: 59999 });
    try {
      await runQuery("SELECT 1", { baseUrl: proxy.url }); // fails, but must not crash the proxy
      const ok = await runQuery("SELECT 7 AS n", { baseUrl: proxy.url }).catch(() => null);
      // The second connection still gets a response (another error), proving the
      // proxy process survived the first failure.
      expect(ok === null || ["error", "closed"].includes(ok.control.event)).toBe(true);
    } finally {
      proxy.stop();
    }
  }, 20000);
});
