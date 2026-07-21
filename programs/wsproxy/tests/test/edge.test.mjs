import { describe, it, expect, beforeEach } from "vitest";
import { runQuery, Session, backendScalar } from "./helpers.mjs";

describe("edge cases", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  it("handles an empty result set", async () => {
    const { text, control } = await runQuery("SELECT 1 AS n WHERE 0");
    expect(control.event).toBe("end");
    expect(text.split("\n").filter((l) => l.length > 0).length).toBe(0);
  });

  it("handles LIMIT 0 (header only, no rows)", async () => {
    const { text, control } = await runQuery("SELECT number FROM numbers(100) LIMIT 0");
    expect(control.event).toBe("end");
    expect(text.split("\n").filter((l) => l.length > 0).length).toBe(0);
  });

  it("runs DDL and reports end with no data", async () => {
    const create = await runQuery(
      "CREATE TABLE IF NOT EXISTS default.wsp_edge_ddl (x UInt8) ENGINE = Memory",
    );
    expect(create.control.event).toBe("end");
    expect(create.text.trim()).toBe("");
    const drop = await runQuery("DROP TABLE default.wsp_edge_ddl");
    expect(drop.control.event).toBe("end");
  });

  it("handles many columns and NULLs in one row", async () => {
    const { text, control } = await runQuery(
      "SELECT 1 AS a, NULL AS b, 'x' AS c, [1,2] AS d, toFloat64(1.5) AS e",
    );
    expect(control.event).toBe("end");
    const row = JSON.parse(text.trim());
    expect(row.b).toBe(null);
    expect(row.c).toBe("x");
    expect(row.d).toEqual([1, 2]);
  });

  it("handles an empty string and unicode payload round-trip via INSERT", async () => {
    const s = new Session("JSONEachRow");
    const ok = await s.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", [
      '{"a":1,"b":""}\n{"a":2,"b":"日本語"}\n',
    ]);
    s.close();
    expect(ok.control.event).toBe("end");
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
    expect(await backendScalar("SELECT b FROM default.wsp_test WHERE a = 2")).toBe("日本語");
    expect(await backendScalar("SELECT length(b) FROM default.wsp_test WHERE a = 1")).toBe("0");
  });
});
