import { describe, it, expect } from "vitest";
import { Session, runQuery } from "./helpers.mjs";

describe("SELECT", () => {
  it("returns a scalar SELECT as JSONEachRow", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a, 'hello' AS b");
    expect(control.event).toBe("end");
    expect(text).toContain('{"a":1,"b":"hello"}');
  });

  it("streams a small result set as one line per row", async () => {
    const { text, control } = await runQuery("SELECT number FROM numbers(5)");
    expect(control.event).toBe("end");
    const lines = text.split("\n").filter((l) => l.length > 0);
    expect(lines).toHaveLength(5);
  });

  it("respects the output format chosen via ?format=", async () => {
    const sql = "SELECT 42, 'x'";

    const tsv = await runQuery(sql, { format: "TSV" });
    expect(tsv.control.event).toBe("end");
    expect(tsv.text.trim()).toBe("42\tx");

    const csv = await runQuery(sql, { format: "CSV" });
    expect(csv.control.event).toBe("end");
    expect(csv.text).toContain('42,"x"');

    const json = await runQuery(sql, { format: "JSONEachRow" });
    expect(json.control.event).toBe("end");
    const line = json.text.split("\n").find((l) => l.length > 0);
    expect(() => JSON.parse(line)).not.toThrow();
  });

  it("propagates a backend error as an error control event", async () => {
    const { control } = await runQuery("SELECT * FROM default.a_missing_table_zz");
    expect(control.event).toBe("error");
    expect(typeof control.message).toBe("string");
    expect(control.message.length).toBeGreaterThan(0);
    expect(control.message).toMatch(/table|UNKNOWN_TABLE/i);
  });

  it("streams a large result set without dropping rows", async () => {
    const { text, control } = await runQuery("SELECT number FROM numbers(10000)");
    expect(control.event).toBe("end");
    const lines = text.split("\n").filter((l) => l.length > 0);
    expect(lines).toHaveLength(10000);
  });

  it("runs multiple queries over one persistent session", async () => {
    const s = new Session("JSONEachRow");
    try {
      const first = await s.run("SELECT 1 AS n");
      expect(first.control.event).toBe("end");
      expect(first.text).toContain('{"n":1}');

      const second = await s.run("SELECT 2 AS n");
      expect(second.control.event).toBe("end");
      expect(second.text).toContain('{"n":2}');
    } finally {
      s.close();
    }
  });
});
