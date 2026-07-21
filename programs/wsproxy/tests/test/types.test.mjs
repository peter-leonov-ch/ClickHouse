import { describe, it, expect } from "vitest";
import { runQuery, backendScalar, Session } from "./helpers.mjs";

// Round-trip a typed literal through the proxy's format layer, rendering it as
// JSONEachRow, and return the parsed `v` value. This validates that the proxy
// faithfully carries ClickHouse's data types through the output format.
async function selectJSON(expr) {
  const { text, control } = await runQuery(`SELECT ${expr} AS v`, {
    format: "JSONEachRow",
  });
  expect(control.event).toBe("end");
  return JSON.parse(text.trim()).v;
}

describe("type coverage", () => {
  it("round-trips integers and floats", async () => {
    expect(await selectJSON("toInt64(-5)")).toBe(-5);
    // 64-bit integers must survive intact. NOTE: the proxy currently renders
    // UInt64 UNQUOTED in JSONEachRow, whereas clickhouse-client quotes by
    // default (output_format_json_quote_64bit_integers) — a format-settings
    // faithfulness gap for the productionization track (unquoted big ints lose
    // precision when JSON.parsed in JS). Assert on the raw digits so this tests
    // data integrity independent of quoting.
    const u64 = await runQuery("SELECT toUInt64(18446744073709551615) AS v", {
      format: "JSONEachRow",
    });
    expect(u64.control.event).toBe("end");
    expect(u64.text).toContain("18446744073709551615");
    expect(await selectJSON("toFloat64(1.5)")).toBe(1.5);
  });

  it("round-trips a Decimal", async () => {
    // Decimals render as JSON numbers in JSONEachRow.
    expect(await selectJSON("toDecimal64(3.14, 2)")).toBe(3.14);
  });

  it("round-trips strings with unicode and escapes", async () => {
    expect(await selectJSON("'a\\tb\\n\"c\"'")).toBe('a\tb\n"c"');
    expect(await selectJSON("'héllo'")).toBe("héllo");
  });

  it("round-trips date and time types", async () => {
    expect(await selectJSON("toDate('2020-01-02')")).toBe("2020-01-02");
    expect(await selectJSON("toDateTime('2020-01-02 03:04:05')")).toBe(
      "2020-01-02 03:04:05",
    );
    expect(await selectJSON("toDateTime64('2020-01-02 03:04:05.123', 3)")).toBe(
      "2020-01-02 03:04:05.123",
    );
  });

  it("round-trips a UUID", async () => {
    expect(
      await selectJSON("toUUID('00000000-0000-0000-0000-000000000001')"),
    ).toBe("00000000-0000-0000-0000-000000000001");
  });

  it("round-trips IPv4 and IPv6", async () => {
    expect(await selectJSON("toIPv4('1.2.3.4')")).toBe("1.2.3.4");
    expect(await selectJSON("toIPv6('::1')")).toBe("::1");
  });

  it("round-trips an Enum", async () => {
    expect(await selectJSON("CAST('a', 'Enum8(\\'a\\'=1,\\'b\\'=2)')")).toBe("a");
  });

  it("round-trips Nullable (null and non-null)", async () => {
    expect(await selectJSON("CAST(NULL, 'Nullable(String)')")).toBeNull();
    expect(await selectJSON("CAST('present', 'Nullable(String)')")).toBe(
      "present",
    );
  });

  it("round-trips LowCardinality", async () => {
    expect(await selectJSON("CAST('x', 'LowCardinality(String)')")).toBe("x");
  });

  it("round-trips an Array", async () => {
    expect(await selectJSON("[1,2,3]::Array(UInt32)")).toEqual([1, 2, 3]);
  });

  it("round-trips a Map (rendered as a JSON object)", async () => {
    // UInt8 values fit in JS numbers, so no 64-bit quoting here.
    expect(await selectJSON("map('k',1,'j',2)")).toEqual({ k: 1, j: 2 });
  });

  it("round-trips a Tuple", async () => {
    // JSONEachRow renders unnamed tuples as an object keyed by position
    // ({"1":1,"2":"x"}); assert loosely that the values are present.
    const v = await selectJSON("tuple(1,'x')");
    const values = Array.isArray(v) ? v : Object.values(v);
    expect(values).toContain(1);
    expect(values).toContain("x");
  });
});

describe("advanced types", () => {
  it("handles the JSON type", async () => {
    const { text, control } = await runQuery(
      `SELECT '{"a":1,"b":"x"}'::JSON AS v`,
      { format: "JSONEachRow" },
    );
    expect(control.event).toBe("end");
    expect(text).toContain("a");
    expect(text).toContain("1");
  });

  it("handles the Dynamic type", async () => {
    const v = await selectJSON("42::Dynamic");
    // The value survives; may arrive as a number or a string depending on
    // Dynamic's JSON rendering, so just assert it is present and equals 42.
    expect(v).not.toBeUndefined();
    expect(v).not.toBeNull();
    expect(String(v)).toBe("42");
  });

  it("handles the Variant type", async () => {
    // Cast from a type that IS in the Variant (UInt64), not a bare UInt8 literal.
    const { control, text } = await runQuery(
      "SELECT CAST(toUInt64(42), 'Variant(UInt64, String)') AS v",
      { format: "JSONEachRow" },
    );
    expect(control.event).toBe("end");
    expect(text).toContain("42");
  });

  it("serializes an AggregateFunction state via a binary format", async () => {
    // JSONEachRow cannot represent an aggregate state; RowBinary can. This
    // proves the proxy streams aggregate-state bytes without error.
    const { data, control } = await runQuery(
      "SELECT sumState(number) AS v FROM numbers(10)",
      { format: "RowBinary" },
    );
    expect(control.event).toBe("end");
    expect(data.length).toBeGreaterThan(0);
  });
});

describe("INSERT round-trip", () => {
  // Prove the INPUT path handles complex types end-to-end: create a Memory
  // table, insert one row of a complex type via JSONEachRow, then read it back.
  async function withTable(columnDef, run) {
    const table = `default.wsp_types_${Date.now()}_${Math.floor(Math.random() * 1e6)}`;
    const create = await runQuery(
      `CREATE TABLE ${table} (${columnDef}) ENGINE = Memory`,
    );
    expect(create.control.event).toBe("end");
    try {
      await run(table);
    } finally {
      await runQuery(`DROP TABLE IF EXISTS ${table}`);
    }
  }

  it("round-trips an Array(UInt32) column", async () => {
    await withTable("v Array(UInt32)", async (table) => {
      const s = new Session("JSONEachRow");
      try {
        const { control } = await s.insert(
          `INSERT INTO ${table} FORMAT JSONEachRow`,
          ['{"v":[1,2,3]}\n'],
        );
        expect(control.event).toBe("end");
      } finally {
        s.close();
      }
      expect(await backendScalar(`SELECT v FROM ${table}`)).toBe("[1,2,3]");
    });
  });

  it("round-trips a Nullable(String) column", async () => {
    await withTable("v Nullable(String)", async (table) => {
      const s = new Session("JSONEachRow");
      try {
        const { control } = await s.insert(
          `INSERT INTO ${table} FORMAT JSONEachRow`,
          ['{"v":null}\n{"v":"hi"}\n'],
        );
        expect(control.event).toBe("end");
      } finally {
        s.close();
      }
      // TSV renders SQL NULL as \N; the non-null value round-trips verbatim.
      expect(await backendScalar(`SELECT count() FROM ${table}`)).toBe("2");
      expect(
        await backendScalar(`SELECT v FROM ${table} WHERE v IS NOT NULL`),
      ).toBe("hi");
      expect(
        await backendScalar(
          `SELECT count() FROM ${table} WHERE v IS NULL`,
        ),
      ).toBe("1");
    });
  });
});
