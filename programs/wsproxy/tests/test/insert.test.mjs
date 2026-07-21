import { describe, it, expect, beforeEach } from "vitest";
import { Session, runQuery, backendScalar } from "./helpers.mjs";

describe("INSERT", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  // These INSERTs carry their own data (SELECT source / inline VALUES), so they
  // run as plain queries — no streamed data phase, and the proxy never parses SQL.
  it("runs INSERT ... SELECT as a plain query (no data phase)", async () => {
    const r = await runQuery("INSERT INTO default.wsp_test SELECT number, 'z' FROM numbers(4)");
    expect(r.control.event).toBe("end");
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("4");
  });

  it("runs inline INSERT ... VALUES as a plain query", async () => {
    const r = await runQuery("INSERT INTO default.wsp_test VALUES (1, 'a'), (2, 'b')");
    expect(r.control.event).toBe("end");
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
  });

  it("streams a JSONEachRow insert across multiple chunks", async () => {
    const s = new Session("JSONEachRow");
    try {
      const { control } = await s.insert(
        "INSERT INTO default.wsp_test FORMAT JSONEachRow",
        ['{"a":1,"b":"x"}\n{"a":2,"b":"y"}\n', '{"a":3,"b":"z"}\n'],
      );
      expect(control.event).toBe("end");
    } finally {
      s.close();
    }

    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("3");
    expect(await backendScalar("SELECT sum(a) FROM default.wsp_test")).toBe("6");
  });

  it("handles an inline VALUES insert with an inferred format", async () => {
    const s = new Session("JSONEachRow");
    try {
      const { control } = await s.insert(
        "INSERT INTO default.wsp_test VALUES (10,'v'),(11,'w')",
        [],
      );
      expect(control.event).toBe("end");
    } finally {
      s.close();
    }

    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
    expect(await backendScalar("SELECT sum(a) FROM default.wsp_test")).toBe("21");
  });

  it("streams an insert in a different input format (TSV)", async () => {
    const s = new Session("JSONEachRow");
    try {
      const { control } = await s.insert(
        "INSERT INTO default.wsp_test FORMAT TSV",
        ["5\tp\n6\tq\n"],
        { format: "TSV" }, // input format for the streamed data
      );
      expect(control.event).toBe("end");
    } finally {
      s.close();
    }

    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
  });

  it("reports an error when inserting into a missing table", async () => {
    const s = new Session("JSONEachRow");
    try {
      const { control } = await s.insert(
        "INSERT INTO default.no_such_table_zz FORMAT JSONEachRow",
        ['{"a":1,"b":"x"}\n'],
      );
      expect(control.event).toBe("error");
      expect(control.message).toBeTruthy();
      expect(control.message).toMatch(/table/i);
    } finally {
      s.close();
    }
  });

  it("keeps the connection healthy for a SELECT after inserts", async () => {
    const s = new Session("JSONEachRow");
    try {
      const insertResult = await s.insert(
        "INSERT INTO default.wsp_test FORMAT JSONEachRow",
        ['{"a":1,"b":"x"}\n{"a":2,"b":"y"}\n'],
      );
      expect(insertResult.control.event).toBe("end");
    } finally {
      s.close();
    }

    const { text, control } = await runQuery(
      "SELECT count() FROM default.wsp_test",
    );
    expect(control.event).toBe("end");
    expect(text).toContain("2");
  });
});
