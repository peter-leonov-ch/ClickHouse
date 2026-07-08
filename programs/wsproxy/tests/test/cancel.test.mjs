import { describe, it, expect } from "vitest";
import { Session, backendScalar, sleep } from "./helpers.mjs";

describe("cancel", () => {
  it(
    "cancels a running query server-side",
    async () => {
      // Unique marker so we can find exactly this query in system.processes.
      const marker = "CANCEL_MARKER_" + Date.now();

      // A long (~10s) query that streams slowly, one row per block, with the
      // marker in a trailing comment so it shows up in system.processes.
      const sql = `SELECT sleepEachRow(0.2), number FROM numbers(50) SETTINGS max_block_size = 1 -- ${marker}`;

      // Query that counts how many instances of our marker query are running,
      // excluding the counting query itself.
      const countSql = `SELECT count() FROM system.processes WHERE query LIKE '%${marker}%' AND query NOT LIKE '%system.processes%'`;

      const s = new Session("JSONEachRow");
      await s.ready();
      s.sendQuery(sql);

      // Wait until the query is actually streaming: the first BINARY frame
      // proves a block arrived and the query is running on the backend.
      let streaming = false;
      for (let i = 0; i < 50 && !streaming; i++) {
        const frame = await s.nextFrame();
        if (frame.type === "binary") streaming = true;
        else if (frame.type === "close") break; // socket died unexpectedly
      }
      expect(streaming).toBe(true);

      // Sanity check: the query really is visible on the backend.
      const running = await backendScalar(countSql);
      expect(running).not.toBe("0");

      // Closing the socket cancels the running query.
      const startedAt = Date.now();
      s.close();

      // Poll for up to ~5s until the marker query is gone from the backend.
      let finalCount = running;
      for (let i = 0; i < 20; i++) {
        finalCount = await backendScalar(countSql);
        if (finalCount === "0") break;
        await sleep(250);
      }
      const elapsed = Date.now() - startedAt;

      // The query was cancelled promptly, not left to finish naturally.
      expect(finalCount).toBe("0");
      // ~10s natural completion; cancellation must be much faster.
      expect(elapsed).toBeLessThan(6000);
    },
    20000,
  );

  it("a normal query is unaffected", async () => {
    // The proxy should be perfectly healthy after a cancel.
    const s = new Session("JSONEachRow");
    try {
      const { control } = await s.run("SELECT 1");
      expect(control.event).toBe("end");
    } finally {
      s.close();
    }
  });
});
