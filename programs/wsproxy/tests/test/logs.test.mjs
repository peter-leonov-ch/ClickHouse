import { describe, it, expect } from "vitest";
import { runQuery } from "./helpers.mjs";

describe("logs and profile events", () => {
  it("pushes server log lines when the client opts in via ?logs=", async () => {
    const { logs, control } = await runQuery(
      "SELECT count() FROM numbers(1000000)",
      { logs: "trace" },
    );
    expect(control.event).toBe("end");
    expect(logs.length).toBeGreaterThanOrEqual(1);

    // Each log event carries an array of row objects with a `text` column.
    const rows = logs.flatMap((e) => e.rows);
    expect(rows.length).toBeGreaterThanOrEqual(1);
    expect(typeof rows[0].text).toBe("string");
    expect(rows[0].text.length).toBeGreaterThan(0);
  }, 20000);

  it("does not push logs at the default level", async () => {
    const { logs, control } = await runQuery("SELECT 1");
    expect(control.event).toBe("end");
    expect(logs.length).toBe(0);
  });

  it("pushes profile events during a query", async () => {
    // A slow query gives the server time to emit ProfileEvents packets.
    const { profileEvents, control } = await runQuery(
      "SELECT sleepEachRow(0.1), number FROM numbers(30) SETTINGS max_block_size = 1",
    );
    expect(control.event).toBe("end");
    expect(profileEvents.length).toBeGreaterThanOrEqual(1);

    const rows = profileEvents.flatMap((e) => e.rows);
    expect(rows.length).toBeGreaterThanOrEqual(1);
    // ProfileEvents rows have `name` and `value` columns.
    expect(rows.every((r) => typeof r.name === "string")).toBe(true);
  }, 20000);
});
