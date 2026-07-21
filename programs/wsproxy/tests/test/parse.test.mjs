import { describe, it, expect, beforeEach } from "vitest";
import { Session, runQuery, backendScalar } from "./helpers.mjs";

// Opt-in SQL parsing (`?parse=1`). The proxy never parses SQL by default; with
// this flag it parses each query to (a) report the leading verb + routing
// decision as a {"event":"query",...} frame and (b) auto-route a streamed-data
// INSERT without the client sending an explicit {"cmd":"insert",...} message.
describe("parse mode (?parse=1)", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  it("reports a SELECT as a plain query and streams results", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, text, control } = await s.run("SELECT 1 AS x");
      expect(control.event).toBe("end");
      expect(queryInfo).toEqual({ event: "query", kind: "query", verb: "SELECT" });
      expect(JSON.parse(text.trim())).toEqual({ x: 1 });
    } finally {
      s.close();
    }
  });

  it("reports INSERT ... SELECT as a plain query and inserts server-side", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, control } = await s.run(
        "INSERT INTO default.wsp_test SELECT number, 'z' FROM numbers(4)",
      );
      expect(control.event).toBe("end");
      expect(queryInfo.kind).toBe("query"); // self-contained: no client data phase
      expect(queryInfo.verb).toBe("INSERT");
      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("4");
    } finally {
      s.close();
    }
  });

  it("reports inline INSERT ... VALUES as a plain query", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, control } = await s.run(
        "INSERT INTO default.wsp_test VALUES (1, 'a'), (2, 'b')",
      );
      expect(control.event).toBe("end");
      expect(queryInfo.kind).toBe("query"); // inline data: no client data phase
      expect(queryInfo.verb).toBe("INSERT");
      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
    } finally {
      s.close();
    }
  });

  it("auto-routes a streamed INSERT (no control message needed)", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      await s.ready();
      // Send the INSERT as a plain text frame, then stream the rows. The proxy
      // parses it, decides it needs client data, and reads the binary frames.
      s.sendQuery("INSERT INTO default.wsp_test FORMAT JSONEachRow");
      s.sendData('{"a":10,"b":"p"}\n{"a":11,"b":"q"}\n');
      s.endData();
      const { queryInfo, control } = await s.collect();
      expect(control.event).toBe("end");
      expect(queryInfo).toEqual({ event: "query", kind: "insert", verb: "INSERT" });
      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
    } finally {
      s.close();
    }
  });

  it("picks up the input format from the FORMAT clause for a streamed INSERT", async () => {
    // Session output format is JSONEachRow, but the INSERT declares TSV — the
    // proxy should feed the streamed data through TSV, not the session default.
    const s = new Session("JSONEachRow", { parse: true });
    try {
      await s.ready();
      s.sendQuery("INSERT INTO default.wsp_test FORMAT TSV");
      s.sendData("7\tp\n8\tq\n");
      s.endData();
      const { queryInfo, control } = await s.collect();
      expect(control.event).toBe("end");
      expect(queryInfo.kind).toBe("insert");
      expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("2");
    } finally {
      s.close();
    }
  });

  it("reports DDL with its leading verb", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, control } = await s.run(
        "CREATE TABLE IF NOT EXISTS default.wsp_parse_ddl (x UInt8) ENGINE = Memory",
      );
      expect(control.event).toBe("end");
      expect(queryInfo).toEqual({ event: "query", kind: "query", verb: "CREATE" });
    } finally {
      s.close();
      await runQuery("DROP TABLE IF EXISTS default.wsp_parse_ddl");
    }
  });

  it("recovers the leading verb through leading comments", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, control } = await s.run("-- a leading comment\nSELECT 42 AS x");
      expect(control.event).toBe("end");
      expect(queryInfo.verb).toBe("SELECT");
    } finally {
      s.close();
    }
  });

  it("fails closed when opted-in SQL classification cannot parse the query", async () => {
    const s = new Session("JSONEachRow", { parse: true });
    try {
      const { queryInfo, control } = await s.run("SELECT this is not valid sql (((");
      expect(control.event).toBe("error");
      expect(queryInfo).toBeNull();
      expect(control.message).toMatch(/syntax|parse|expected/i);
    } finally {
      s.close();
    }
  });

  it("does not send a query frame when parse mode is off", async () => {
    // Default session: no ?parse=1, so no {"event":"query"} frame at all.
    const { queryInfo, control } = await runQuery("SELECT 1 AS x");
    expect(control.event).toBe("end");
    expect(queryInfo).toBeNull();
  });
});
