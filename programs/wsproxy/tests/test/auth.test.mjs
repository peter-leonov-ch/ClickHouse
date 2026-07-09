import { describe, it, expect } from "vitest";
import { runQuery } from "./helpers.mjs";
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

  it("rejects a wrong password sent via Basic header", async () => {
    const basic = Buffer.from(`${USER}:wrong`).toString("base64");
    const { control } = await currentUserViaHeaders({ Authorization: `Basic ${basic}` });
    expect(control.event).toBe("error");
    expect(control.message).toMatch(/auth|password|ACCESS|denied/i);
  });
});
