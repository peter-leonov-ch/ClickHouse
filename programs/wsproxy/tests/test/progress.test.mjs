import { describe, it, expect } from "vitest";
import { runQuery } from "./helpers.mjs";

/** True if the numbers are in non-decreasing order (compare against a sorted copy). */
function isMonotonicNonDecreasing(values) {
  const sorted = [...values].sort((a, b) => a - b);
  return values.every((v, i) => v === sorted[i]);
}

describe("progress", () => {
  it(
    "pushes progress events during a slow query",
    async () => {
      // ~3s query producing many small blocks so the server emits several
      // progress updates while it runs.
      const sql =
        "SELECT sleepEachRow(0.1), number FROM numbers(30) SETTINGS max_block_size = 1";
      const { progress, control } = await runQuery(sql);

      expect(control.event).toBe("end");

      // Multiple mid-query pushes, not just a single terminal one.
      expect(progress.length).toBeGreaterThanOrEqual(2);

      for (const ev of progress) {
        expect(typeof ev.read_rows).toBe("number");
        expect(typeof ev.read_bytes).toBe("number");
        expect(typeof ev.total_rows_to_read).toBe("number");
      }

      // Running totals never go backwards.
      const readRows = progress.map((ev) => ev.read_rows);
      expect(isMonotonicNonDecreasing(readRows)).toBe(true);

      const last = progress[progress.length - 1];
      expect(last.read_rows).toBe(30);
      expect(last.total_rows_to_read).toBe(30);
    },
    20000,
  );

  it("still ends cleanly for an instant query", async () => {
    const { control } = await runQuery("SELECT 1");
    expect(control.event).toBe("end");
  });
});
