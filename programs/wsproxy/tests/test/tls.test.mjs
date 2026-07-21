import { describe, it, expect } from "vitest";
import { runQuery, Session } from "./helpers.mjs";
import { spawnBackend, spawnProxy } from "./proc.mjs";

// Stands up its own TLS-enabled ClickHouse (self-signed cert) plus a proxy
// configured to connect to it over the secure native protocol.
describe("proxy -> backend TLS", () => {
  it("round-trips a query over a TLS backend connection", async () => {
    const backend = await spawnBackend({ tcpPort: 9003, securePort: 9444 });
    const proxy = await spawnProxy({
      listenPort: 9015,
      backendPort: 9444,
      secure: true,
      acceptInvalidCert: true, // self-signed cert
    });
    try {
      const { text, control } = await runQuery("SELECT 'tls' AS s, currentUser() AS u", {
        baseUrl: proxy.url,
      });
      expect(control.event).toBe("end");
      const row = JSON.parse(text.trim());
      expect(row.s).toBe("tls");
      expect(row.u).toBe("default");
    } finally {
      proxy.stop();
      backend.stop();
    }
  }, 60000);

  it("passes credentials through over TLS", async () => {
    const backend = await spawnBackend({ tcpPort: 9004, securePort: 9445 });
    const proxy = await spawnProxy({
      listenPort: 9016,
      backendPort: 9445,
      secure: true,
      acceptInvalidCert: true,
    });
    try {
      // Correct credentials over TLS.
      const ok = await runQuery("SELECT currentUser() AS u", {
        baseUrl: proxy.url,
        user: "wsp_user",
        password: "wsp_pass",
      });
      expect(ok.control.event).toBe("end");
      expect(JSON.parse(ok.text.trim()).u).toBe("wsp_user");

      // Wrong password is still rejected over TLS.
      const bad = await runQuery("SELECT 1", {
        baseUrl: proxy.url,
        user: "wsp_user",
        password: "nope",
      });
      expect(bad.control.event).toBe("error");
    } finally {
      proxy.stop();
      backend.stop();
    }
  }, 60000);
});
