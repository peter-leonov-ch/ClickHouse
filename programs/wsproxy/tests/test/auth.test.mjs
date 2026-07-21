import { describe, it, expect } from "vitest";
import { runQuery, Session } from "./helpers.mjs";
import { RawClient } from "./raw.mjs";

// The test backend (tmp/ch/users.xml) has a password-protected user:
const USER = "wsp_user";
const PASS = "wsp_pass";

// Run `SELECT currentUser()` through a raw client that authenticates via the
// given HTTP request headers; returns the authenticated user name.
async function currentUserViaHeaders(headers) {
  const c = new RawClient({ port: 9010 });
  await c.handshake("/?format=JSONEachRow", headers);
  c.sendFrame({ opcode: 0x1, payload: "SELECT currentUser() AS u" });
  const chunks = [];
  let control = null;
  for (;;) {
    const f = await c.readFrame();
    if (f.closed) break;
    if (f.opcode === 0x2) chunks.push(f.payload);
    else if (f.opcode === 0x1) {
      const ev = JSON.parse(f.payload.toString());
      if (ev.event === "progress" || ev.event === "log" || ev.event === "profile_events") continue;
      control = ev;
      break;
    }
  }
  c.close();
  const text = Buffer.concat(chunks).toString();
  return { control, user: text.trim() ? JSON.parse(text.trim()).u : null };
}

describe("auth (credential pass-through)", () => {
  it("rejects a cross-origin browser handshake", async () => {
    const c = new RawClient({ port: 9010 });
    const { head } = await c.handshakeResponse("/?format=JSONEachRow", {
      Origin: "https://attacker.example",
    });
    expect(head).toMatch(/^HTTP\/1\.1 403 /);
    expect(head).not.toContain("101 Switching Protocols");
    c.close();
  });

  it("allows a browser origin from the configured allowlist", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow", { Origin: "https://trusted.example" });
    c.sendFrame({ opcode: 0x1, payload: "SELECT 1 AS n" });
    let sawEnd = false;
    for (;;) {
      const f = await c.readFrame();
      if (f.closed) break;
      if (f.opcode === 0x1 && JSON.parse(f.payload.toString()).event === "end") {
        sawEnd = true;
        break;
      }
    }
    expect(sawEnd).toBe(true);
    c.close();
  });

  it("allows a non-browser handshake without an Origin header", async () => {
    const c = new RawClient({ port: 9010 });
    await c.handshake("/?format=JSONEachRow", { Origin: undefined });
    c.sendFrame({ opcode: 0x1, payload: "SELECT 1 AS n" });
    let sawEnd = false;
    for (;;) {
      const f = await c.readFrame();
      if (f.closed) break;
      if (f.opcode === 0x1 && JSON.parse(f.payload.toString()).event === "end") {
        sawEnd = true;
        break;
      }
    }
    expect(sawEnd).toBe(true);
    c.close();
  });

  it("rejects malformed explicit Basic credentials without falling back", async () => {
    const c = new RawClient({ port: 9010 });
    const { head } = await c.handshakeResponse("/?format=JSONEachRow", {
      Authorization: "Basic !!!not-base64!!!",
      "X-ClickHouse-User": USER,
      "X-ClickHouse-Key": PASS,
    });
    expect(head).toMatch(/^HTTP\/1\.1 400 /);
    expect(head).not.toContain("101 Switching Protocols");
    c.close();
  });

  it("authenticates as the default user when no credentials are given", async () => {
    const { text, control } = await runQuery("SELECT currentUser() AS u");
    expect(control.event).toBe("end");
    expect(JSON.parse(text.trim()).u).toBe("default");
  });

  it("authenticates via ?user=/?password= URL params", async () => {
    const { text, control } = await runQuery("SELECT currentUser() AS u", {
      user: USER,
      password: PASS,
    });
    expect(control.event).toBe("end");
    expect(JSON.parse(text.trim()).u).toBe(USER);
  });

  it("rejects a wrong password (backend auth error passed through)", async () => {
    const { control } = await runQuery("SELECT 1", { user: USER, password: "wrong" });
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/auth|password|ACCESS|denied/i);
  });

  it("authenticates via Authorization: Basic header", async () => {
    const basic = Buffer.from(`${USER}:${PASS}`).toString("base64");
    const { control, user } = await currentUserViaHeaders({ Authorization: `Basic ${basic}` });
    expect(control.event).toBe("end");
    expect(user).toBe(USER);
  });

  it("authenticates via X-ClickHouse-User / X-ClickHouse-Key headers", async () => {
    const { control, user } = await currentUserViaHeaders({
      "X-ClickHouse-User": USER,
      "X-ClickHouse-Key": PASS,
    });
    expect(control.event).toBe("end");
    expect(user).toBe(USER);
  });

  it("rejects bad credentials eagerly, before any query is sent", async () => {
    const s = new Session("JSONEachRow", { user: USER, password: "wrong" });
    await s.ready();
    // Do NOT send a query. Eager connect authenticates at session start, so the
    // failure must arrive without the client sending anything.
    let event = null;
    for (let i = 0; i < 4; i++) {
      const f = await s.nextFrame();
      if (f.type === "text") {
        event = JSON.parse(f.data).event;
        break;
      }
      if (f.type === "close") break;
    }
    s.close();
    expect(event).toBe("error");
  }, 10000);

  it("rejects a wrong password sent via Basic header", async () => {
    const basic = Buffer.from(`${USER}:wrong`).toString("base64");
    const { control } = await currentUserViaHeaders({ Authorization: `Basic ${basic}` });
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/auth|password|ACCESS|denied/i);
  });
});
