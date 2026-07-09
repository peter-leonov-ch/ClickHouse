import { describe, it, expect } from "vitest";
import { runQuery } from "./helpers.mjs";

// Converting between ClickHouse output formats is the proxy's whole purpose,
// so exercise a broad spread of them: fixed-byte binary formats, text formats
// with and without headers, structured JSON, and container formats with magic
// bytes (Parquet, Arrow). The row-per-line formats are covered elsewhere.
describe("output formats", () => {
  it("RowBinary emits exact little-endian bytes", async () => {
    const u32 = await runQuery("SELECT toUInt32(42) AS v", { format: "RowBinary" });
    expect(u32.control.event).toBe("end");
    expect(u32.data.equals(Buffer.from([42, 0, 0, 0]))).toBe(true);

    const two = await runQuery("SELECT toUInt8(1), toUInt8(2)", { format: "RowBinary" });
    expect(two.control.event).toBe("end");
    expect(two.data.equals(Buffer.from([1, 2]))).toBe(true);
  });

  it("TabSeparated / TSV separates columns with tabs", async () => {
    const tsv = await runQuery("SELECT 1 AS a, 'x' AS b", { format: "TabSeparated" });
    expect(tsv.control.event).toBe("end");
    expect(tsv.text.trim()).toBe("1\tx");

    const tsvAlias = await runQuery("SELECT 1 AS a, 'x' AS b", { format: "TSV" });
    expect(tsvAlias.control.event).toBe("end");
    expect(tsvAlias.text.trim()).toBe("1\tx");
  });

  it("TSVWithNames prepends a header row", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a, 'x' AS b", {
      format: "TSVWithNames",
    });
    expect(control.event).toBe("end");
    const lines = text.split("\n");
    expect(lines[0]).toBe("a\tb");
    expect(lines[1]).toBe("1\tx");
  });

  it("CSV quotes strings", async () => {
    const { text, control } = await runQuery("SELECT 1, 'x'", { format: "CSV" });
    expect(control.event).toBe("end");
    expect(text).toContain('1,"x"');
  });

  it("CSVWithNames prepends a header row", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a, 'x' AS b", {
      format: "CSVWithNames",
    });
    expect(control.event).toBe("end");
    const lines = text.split("\n").filter((l) => l.length > 0);
    expect(lines[0]).toContain('"a"');
    expect(lines[0]).toContain('"b"');
  });

  it("JSON produces a full object with meta and data", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a", { format: "JSON" });
    expect(control.event).toBe("end");
    const obj = JSON.parse(text);
    expect(Array.isArray(obj.data)).toBe(true);
    expect(Array.isArray(obj.meta)).toBe(true);
    expect(obj.data[0].a).toBeDefined();
  });

  it("JSONCompactEachRow emits an array per row", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a, 'x' AS b", {
      format: "JSONCompactEachRow",
    });
    expect(control.event).toBe("end");
    const line = text.split("\n").find((l) => l.length > 0);
    expect(JSON.parse(line)).toEqual([1, "x"]);
  });

  it("Values renders a tuple literal", async () => {
    const { text, control } = await runQuery("SELECT 1, 'x'", { format: "Values" });
    expect(control.event).toBe("end");
    expect(text.trim()).toBe("(1,'x')");
  });

  it("Pretty renders a table (smoke)", async () => {
    const { text, control } = await runQuery("SELECT 1 AS a", { format: "Pretty" });
    expect(control.event).toBe("end");
    expect(text.length).toBeGreaterThan(0);
    // Box-drawing characters and/or the column name should be present.
    expect(text.includes("┏") || text.includes("─") || text.includes("a")).toBe(true);
  });

  it("Native returns non-empty binary (smoke)", async () => {
    const { data, control } = await runQuery("SELECT number FROM numbers(3)", {
      format: "Native",
    });
    expect(control.event).toBe("end");
    expect(data.length).toBeGreaterThan(0);
  });

  it("Parquet is framed by PAR1 magic bytes", async () => {
    const { data, control } = await runQuery("SELECT number FROM numbers(10)", {
      format: "Parquet",
    });
    expect(control.event).toBe("end");
    expect(data.length).toBeGreaterThan(8);
    expect(data.subarray(0, 4).toString("latin1")).toBe("PAR1");
    expect(data.subarray(-4).toString("latin1")).toBe("PAR1");
  });

  it("Arrow starts with the ARROW1 file magic", async () => {
    const { data, control } = await runQuery("SELECT number FROM numbers(10)", {
      format: "Arrow",
    });
    expect(control.event).toBe("end");
    expect(data.length).toBeGreaterThan(0);
    expect(data.subarray(0, 6).toString("latin1")).toBe("ARROW1");
  });

  it("an unknown format surfaces an error cleanly", async () => {
    const { control } = await runQuery("SELECT 1", { format: "NoSuchFormat" });
    expect(control.event).toBe("error");
  });
});
