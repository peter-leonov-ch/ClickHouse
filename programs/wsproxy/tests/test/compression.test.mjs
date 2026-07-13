import { describe, it, expect, afterAll } from "vitest";
import { spawnProxy } from "./proc.mjs";
import { Session } from "./helpers.mjs";

// The native result-block codec (backend -> proxy) is configurable via
// WSPROXY_BACKEND_COMPRESSION (lz4 default | zstd | none). It only changes bytes
// on the wire and backend/proxy CPU; the data delivered to the client must be
// byte-identical across codecs. On a bandwidth-limited WAN, zstd is much smaller
// than lz4 (columnar native compresses far better than row JSON), which is what
// lets the proxy beat gzipped HTTP — see programs/wsproxy/bench/.
describe("backend compression codec", () => {
  const BACKEND_PORT = 19000;
  const QUERY = "SELECT number AS n, number*2 AS d, toString(number) AS s FROM numbers(50000)";
  const proxies = [];

  afterAll(() => proxies.forEach((p) => p.stop()));

  async function fetchVia(compression, listenPort) {
    const proxy = await spawnProxy({ listenPort, backendPort: BACKEND_PORT, compression });
    proxies.push(proxy);
    const s = new Session("JSONCompactEachRow", { baseUrl: proxy.url });
    try {
      const { text, control } = await s.run(QUERY);
      expect(control.event).toBe("end");
      return text;
    } finally {
      s.close();
    }
  }

  it("delivers byte-identical output for lz4, zstd, and none", async () => {
    const [lz4, zstd, none] = await Promise.all([
      fetchVia("lz4", 9031),
      fetchVia("zstd", 9032),
      fetchVia("none", 9033),
    ]);
    expect(zstd).toBe(lz4);
    expect(none).toBe(lz4);
    // sanity: the payload is non-trivial
    expect(lz4.split("\n").filter((l) => l).length).toBe(50000);
  }, 30000);
});
