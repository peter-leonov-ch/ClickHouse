import { describe, it, expect, beforeEach } from "vitest";
import { runQuery, Session, backendScalar, sleep } from "./helpers.mjs";

describe("insert flow", () => {
  beforeEach(async () => {
    await runQuery("TRUNCATE TABLE default.wsp_test");
  });

  it("aborts an INSERT (no commit) when the client closes mid-stream", async () => {
    const s = new Session("JSONEachRow");
    await s.ready();
    s.sendQuery("INSERT INTO default.wsp_test FORMAT JSONEachRow");
    // Send some data but NEVER send the end marker; close the socket instead.
    s.sendData('{"a":1,"b":"x"}\n{"a":2,"b":"y"}\n');
    await sleep(200);
    s.close(); // Close mid-stream -> proxy calls sendCancel, backend rolls back.

    // Give the backend a moment to process the cancel, then verify nothing committed.
    await sleep(500);
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("0");
  }, 20000);

  it("commits a normal INSERT after a mid-stream abort (proxy stays healthy)", async () => {
    // Abort one insert...
    const a = new Session("JSONEachRow");
    await a.ready();
    a.sendQuery("INSERT INTO default.wsp_test FORMAT JSONEachRow");
    a.sendData('{"a":9,"b":"q"}\n');
    await sleep(150);
    a.close();
    await sleep(300);

    // ...then a clean insert still works.
    const b = new Session("JSONEachRow");
    const ok = await b.insert("INSERT INTO default.wsp_test FORMAT JSONEachRow", ['{"a":1,"b":"x"}\n']);
    b.close();
    expect(ok.control.event).toBe("end");
    expect(await backendScalar("SELECT count() FROM default.wsp_test")).toBe("1");
  }, 20000);
});
