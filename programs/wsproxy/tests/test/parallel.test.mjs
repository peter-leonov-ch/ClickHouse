import { describe, it, expect } from "vitest";
import { Session } from "./helpers.mjs";

// Opt-in parallel output formatting (`?parallel=1`). Formats SELECT output on a
// thread pool for throughput; result frames are coarser (blocks are batched) but
// the delivered bytes must be identical to the default single-threaded path.
describe("parallel output formatting (?parallel=1)", () => {
  const QUERY = "SELECT number AS n, toString(number) AS s FROM numbers(200000)";

  async function fetchText(opts) {
    const s = new Session("JSONCompactEachRow", opts);
    try {
      const { text, control } = await s.run(QUERY);
      expect(control.event).toBe("end");
      return text;
    } finally {
      s.close();
    }
  }

  it("produces byte-identical output to the default path", async () => {
    const [plain, parallel] = await Promise.all([fetchText({}), fetchText({ parallel: true })]);
    expect(parallel.length).toBe(plain.length);
    expect(parallel).toBe(plain);
  });

  it("delivers all rows", async () => {
    const s = new Session("JSONCompactEachRow", { parallel: true });
    try {
      const { text, control } = await s.run(QUERY);
      expect(control.event).toBe("end");
      const lines = text.split("\n").filter((l) => l.length > 0);
      expect(lines.length).toBe(200000);
      expect(JSON.parse(lines[0])).toEqual([0, "0"]);
      expect(JSON.parse(lines[199999])).toEqual([199999, "199999"]);
    } finally {
      s.close();
    }
  });

  it("propagates a backend exception as an error event", async () => {
    const s = new Session("JSONCompactEachRow", { parallel: true });
    try {
      const { control } = await s.run("SELECT throwIf(number = 5) FROM numbers(100)");
      expect(control.event).toBe("error");
      expect(control.message).toBeTruthy();
    } finally {
      s.close();
    }
  });
});
