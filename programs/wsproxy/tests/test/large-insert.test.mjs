import { describe, it, expect, beforeEach } from "vitest";
import { Session, runQuery, backendScalar } from "./helpers.mjs";

describe("large INSERT", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  it(
    "streams a JSONEachRow insert of 500,000 rows across multiple frames",
    async () => {
      const total = 500000;
      const rowsPerChunk = 50000;
      const chunks = [];
      for (let start = 0; start < total; start += rowsPerChunk) {
        const rows = [];
        for (let i = start; i < start + rowsPerChunk; i++) {
          rows.push(`{"a":${i},"b":"r${i}"}\n`);
        }
        chunks.push(rows.join(""));
      }
      expect(chunks.length).toBe(10);

      const s = new Session("JSONEachRow");
      let control;
      try {
        ({ control } = await s.insert(
          "INSERT INTO default.wsp_test FORMAT JSONEachRow",
          chunks,
        ));
      } finally {
        s.close();
      }
      expect(control.event).toBe("end");

      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe(
        "500000",
      );
      // Exact sum of 0..499999, computed with BigInt to avoid float precision issues.
      const expectedSum = ((499999n * 500000n) / 2n).toString();
      expect(expectedSum).toBe("124999750000");
      expect(await backendScalar("SELECT sum(a) FROM default.wsp_test")).toBe(
        expectedSum,
      );
    },
    60000,
  );

  it(
    "handles a large INSERT in a single big frame",
    async () => {
      const total = 100000;
      const rows = [];
      for (let i = 0; i < total; i++) {
        rows.push(`{"a":${i},"b":"r${i}"}\n`);
      }
      const chunk = rows.join("");

      const s = new Session("JSONEachRow");
      let control;
      try {
        ({ control } = await s.insert(
          "INSERT INTO default.wsp_test FORMAT JSONEachRow",
          [chunk],
        ));
      } finally {
        s.close();
      }
      expect(control.event).toBe("end");

      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe(
        "100000",
      );
    },
    60000,
  );

  it(
    "round-trips integrity via a different input format (TSV)",
    async () => {
      const total = 100000;
      const rows = [];
      for (let i = 0; i < total; i++) {
        rows.push(`${i}\tr${i}\n`);
      }
      const chunk = rows.join("");

      const s = new Session("JSONEachRow");
      let control;
      try {
        ({ control } = await s.insert(
          "INSERT INTO default.wsp_test FORMAT TSV",
          [chunk],
          { format: "TSV" }, // input format for the streamed data
        ));
      } finally {
        s.close();
      }
      expect(control.event).toBe("end");

      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe(
        "100000",
      );
      expect(await backendScalar("SELECT max(a) FROM default.wsp_test")).toBe(
        "99999",
      );
    },
    60000,
  );
});
