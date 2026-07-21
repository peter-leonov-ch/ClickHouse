import { describe, it, expect, beforeEach } from "vitest";
import { runQuery, Session, backendScalar } from "./helpers.mjs";

describe("exceptions", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  it("streams partial results, then delivers the exception", async () => {
    // throwIf fires at number = 5, so rows 0..4 stream first, then the error.
    const { text, control } = await runQuery(
      "SELECT number, throwIf(number = 5, 'boom') AS t FROM numbers(10) SETTINGS max_block_size = 1",
    );
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/boom/);
    const lines = text.split("\n").filter((l) => l.length > 0);
    expect(lines.length).toBe(5); // partial results were delivered before the error
    // First streamed row is number 0 (quoting-agnostic: the proxy renders UInt64 unquoted).
    expect(String(JSON.parse(lines[0]).number)).toBe("0");
  });

  it("reuses the session after a mid-stream exception", async () => {
    const s = new Session("JSONEachRow");
    const bad = await s.run(
      "SELECT throwIf(number = 3, 'x') FROM numbers(10) SETTINGS max_block_size = 1",
    );
    expect(bad.control.event).toBe("error");
    // The reused Connection must survive an exception that occurred mid-stream.
    const good = await s.run("SELECT 1 AS n");
    expect(good.control.event).toBe("end");
    expect(JSON.parse(good.text.trim()).n).toBe(1);
    s.close();
  });

  it("aborts an INSERT on malformed data without committing partial rows", async () => {
    const s = new Session("JSONEachRow");
    const { control } = await s.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", [
      '{"a":1,"b":"x"}\n',
      "NOT JSON AT ALL\n",
    ]);
    s.close();
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/Cannot parse|parse input|Code: 27/);
    // The valid first row must NOT be committed — the insert aborted.
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("0");
  });

  it("reports a type error in INSERT data", async () => {
    const s = new Session("JSONEachRow");
    const { control } = await s.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", [
      '{"a":"notanumber","b":"x"}\n',
    ]);
    s.close();
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/parse|Cannot|Code: 27/);
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("0");
  });

  it("preserves the ClickHouse error message and code", async () => {
    const { control } = await runQuery("SELECT * FROM default.definitely_missing_tbl_xyz");
    expect(control.event).toBe("error");
    expect(control.message).toContain("definitely_missing_tbl_xyz");
    expect(control.message).toMatch(/Unknown table|UNKNOWN_TABLE|Code: 60|Code: 47/);
  });

  it("keeps the session healthy after an INSERT error", async () => {
    const s = new Session("JSONEachRow");
    const bad = await s.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", ["garbage\n"]);
    expect(bad.control.event).toBe("error");
    s.close();
    // A fresh session works, and a valid insert then commits.
    const s2 = new Session("JSONEachRow");
    const ok = await s2.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", ['{"a":7,"b":"z"}\n']);
    s2.close();
    expect(ok.control.event).toBe("end");
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("1");
  });
});
