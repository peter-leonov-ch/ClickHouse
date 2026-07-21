import { describe, it, expect } from "vitest";
import { Session, runQuery } from "./helpers.mjs";

// These tests stress the proxy with many simultaneous sessions, long-lived
// sessions, and large streamed results. The proxy uses one native Connection
// per WebSocket session and a Poco thread pool (capacity ~16), so this
// exercises thread-per-session behaviour under load and result streaming.

const countLines = (text) => text.split("\n").filter((l) => l.length > 0).length;

describe("concurrency", () => {
  it("handles many concurrent independent sessions", async () => {
    const N = 40;
    const results = await Promise.all(
      Array.from({ length: N }, (_, i) => runQuery(`SELECT ${i} AS n`)),
    );

    expect(results).toHaveLength(N);
    results.forEach((res, i) => {
      expect(res.control.event).toBe("end");
      expect(JSON.parse(res.text.trim()).n).toBe(i);
    });
  }, 30000);

  it("handles concurrent slow queries through the thread pool", async () => {
    const N = 12;
    const sql =
      "SELECT sleepEachRow(0.05), number FROM numbers(10) SETTINGS max_block_size = 1";
    const results = await Promise.all(
      Array.from({ length: N }, () => runQuery(sql)),
    );

    expect(results).toHaveLength(N);
    for (const res of results) {
      expect(res.control.event).toBe("end");
    }
  }, 30000);

  it("survives many sequential queries on one long-lived session", async () => {
    const N = 100;
    const s = new Session("JSONEachRow");
    try {
      let last;
      for (let i = 0; i < N; i++) {
        last = await s.run(`SELECT ${i} AS n`);
        expect(last.control.event).toBe("end");
      }
      expect(JSON.parse(last.text.trim()).n).toBe(N - 1);
    } finally {
      s.close();
    }
  }, 30000);

  it("streams a large result set without dropping rows", async () => {
    const { text, control } = await runQuery("SELECT number FROM numbers(1000000)");
    expect(control.event).toBe("end");
    expect(countLines(text)).toBe(1000000);
  }, 30000);

  it("handles concurrent large-ish result sets", async () => {
    const N = 5;
    const results = await Promise.all(
      Array.from({ length: N }, () => runQuery("SELECT number FROM numbers(100000)")),
    );

    expect(results).toHaveLength(N);
    for (const res of results) {
      expect(res.control.event).toBe("end");
      expect(countLines(res.text)).toBe(100000);
    }
  }, 30000);
});
