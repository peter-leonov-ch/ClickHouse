// WebSocket client helpers for the clickhouse-wsproxy integration tests.
//
// Protocol recap (proxy <-> client over one WebSocket):
//   - The client sends a SQL query as a TEXT frame.
//   - SELECT results stream back as BINARY frames (bytes of the chosen output
//     format, set via the `?format=` query parameter of the WS URL).
//   - Mid-query progress is pushed as TEXT frames: {"event":"progress",...}.
//   - The query terminates with a TEXT control frame:
//       {"event":"end"} | {"event":"error","message":...} | {"event":"cancelled"}
//   - INSERT: send `INSERT INTO t [FORMAT X]` as a TEXT frame, then stream data
//     as BINARY frames, then a zero-length BINARY frame to signal end of data.
//   - Cancel a running query by CLOSING the socket.
//
// Uses Node's built-in global WebSocket (Node >= 22), so no dependencies.

export const PROXY_URL = process.env.WSPROXY_URL ?? "ws://127.0.0.1:9010";

/** Build the WS URL for a format and options ({ logs, path, baseUrl, user, password, flow, parse }). */
export function urlFor(
  format = "JSONEachRow",
  { logs = "", path = "/", baseUrl = PROXY_URL, user = "", password = "", flow, parse = false } = {},
) {
  const params = new URLSearchParams({ format });
  if (logs) params.set("logs", logs);
  if (user) params.set("user", user);
  if (password) params.set("password", password);
  if (flow !== undefined) params.set("flow", String(flow)); // opt-in credit flow control
  if (parse) params.set("parse", "1"); // opt-in SQL parsing (auto-route inserts, report kind)
  return `${baseUrl}${path}?${params}`;
}

/**
 * A single proxy session over one WebSocket. Supports multiple sequential
 * queries. Incoming frames are buffered into an async queue so callers can
 * `await` them regardless of arrival timing.
 */
export class Session {
  constructor(
    format = "JSONEachRow",
    { logs = "", path = "/", baseUrl = PROXY_URL, user = "", password = "", flow, parse = false } = {},
  ) {
    this.ws = new WebSocket(urlFor(format, { logs, path, baseUrl, user, password, flow, parse }));
    this.ws.binaryType = "arraybuffer";
    this._queue = [];
    this._waiters = [];
    this._closed = false;

    const push = (frame) => {
      const waiter = this._waiters.shift();
      if (waiter) waiter(frame);
      else this._queue.push(frame);
    };

    this.ws.addEventListener("message", (ev) => {
      if (typeof ev.data === "string") push({ type: "text", data: ev.data });
      else push({ type: "binary", data: Buffer.from(ev.data) });
    });
    this.ws.addEventListener("close", () => {
      this._closed = true;
      push({ type: "close" });
    });

    this.opened = new Promise((resolve, reject) => {
      this.ws.addEventListener("open", () => resolve());
      this.ws.addEventListener("error", (ev) =>
        reject(new Error(`WebSocket error: ${ev?.message ?? ev}`)),
      );
    });
  }

  /** Resolve once the socket is open. */
  ready() {
    return this.opened;
  }

  /**
   * Next incoming frame: {type:'text',data:string} | {type:'binary',data:Buffer} | {type:'close'}.
   * With `timeoutMs > 0`, resolves to null if no frame arrives in time (without
   * losing a frame that arrives later — the waiter is removed on timeout).
   */
  nextFrame(timeoutMs = 0) {
    if (this._queue.length) return Promise.resolve(this._queue.shift());
    return new Promise((resolve) => {
      let settled = false;
      const waiter = (frame) => {
        if (settled) {
          this._queue.unshift(frame); // raced with the timeout; don't drop it
          return;
        }
        settled = true;
        resolve(frame);
      };
      this._waiters.push(waiter);
      if (timeoutMs > 0) {
        setTimeout(() => {
          if (settled) return;
          settled = true;
          const i = this._waiters.indexOf(waiter);
          if (i >= 0) this._waiters.splice(i, 1);
          resolve(null);
        }, timeoutMs);
      }
    });
  }

  /** Flow-control: grant N more frames of credit. */
  next(n) {
    this.ws.send(JSON.stringify({ cmd: "next", n }));
  }

  /** Flow-control: pause the server push. */
  pause() {
    this.ws.send(JSON.stringify({ cmd: "pause" }));
  }

  /** Flow-control: resume after pause(). */
  resume() {
    this.ws.send(JSON.stringify({ cmd: "resume" }));
  }

  /** Send a SQL query (TEXT frame). */
  sendQuery(sql) {
    this.ws.send(sql);
  }

  /** Send INSERT data (BINARY frame). Accepts a string or a Buffer/Uint8Array. */
  sendData(chunk) {
    this.ws.send(typeof chunk === "string" ? Buffer.from(chunk) : chunk);
  }

  /** Signal end of INSERT data (zero-length BINARY frame). */
  endData() {
    this.ws.send(new Uint8Array(0));
  }

  /**
   * Consume frames until the terminal control event.
   * Returns { data: Buffer, text: string, progress: object[], control: object }.
   * Binary frames are concatenated into `data`; progress events collected.
   */
  async collect() {
    const chunks = [];
    const progress = [];
    const logs = [];
    const profileEvents = [];
    let queryInfo = null; // {"event":"query",...} sent in ?parse=1 mode
    const done = (control) => {
      const data = Buffer.concat(chunks);
      return { data, text: data.toString("utf8"), progress, logs, profileEvents, queryInfo, control };
    };
    for (;;) {
      const frame = await this.nextFrame();
      if (frame.type === "binary") {
        chunks.push(frame.data);
      } else if (frame.type === "text") {
        const msg = JSON.parse(frame.data);
        // Terminal events end the query; everything else is a mid-query push.
        if (msg.event === "end" || msg.event === "error" || msg.event === "cancelled") {
          return done(msg);
        }
        if (msg.event === "progress") progress.push(msg);
        else if (msg.event === "log") logs.push(msg);
        else if (msg.event === "profile_events") profileEvents.push(msg);
        else if (msg.event === "query") queryInfo = msg;
        // Unknown non-terminal events are ignored.
      } else {
        // Socket closed without a terminal control frame.
        return done({ event: "closed" });
      }
    }
  }

  /** Send a query and collect its full result. */
  async run(sql) {
    await this.ready();
    this.sendQuery(sql);
    return this.collect();
  }

  /**
   * Begin a streamed INSERT: declare it with a control message so the proxy
   * opts into the data phase (the proxy never parses SQL). `format` is the input
   * format of the data you'll stream (defaults to the session's format).
   */
  beginInsert(query, { format } = {}) {
    const cmd = { cmd: "insert", query };
    if (format) cmd.format = format;
    this.ws.send(JSON.stringify(cmd));
  }

  /**
   * Run a streamed INSERT: declare it, stream the data chunks, end, and collect
   * the terminal control event. `chunks` is an array of strings/Buffers.
   */
  async insert(query, chunks = [], opts = {}) {
    await this.ready();
    this.beginInsert(query, opts);
    for (const c of chunks) this.sendData(c);
    this.endData();
    return this.collect();
  }

  /** Close the socket (also used to cancel a running query). */
  close() {
    try {
      this.ws.close();
    } catch {
      /* already closing */
    }
  }
}

/** One-shot: open a session, run a query, close, return the result object. */
export async function runQuery(
  sql,
  { format = "JSONEachRow", logs = "", baseUrl = PROXY_URL, user = "", password = "" } = {},
) {
  const s = new Session(format, { logs, baseUrl, user, password });
  try {
    return await s.run(sql);
  } finally {
    s.close();
  }
}

/**
 * Query the backend through the proxy and return the single scalar/text result,
 * trimmed. Handy for verifying server-side state (uses TSV so there is no JSON
 * quoting to strip).
 */
export async function backendScalar(sql) {
  const { text, control } = await runQuery(sql, { format: "TSV" });
  if (control.event !== "end") {
    throw new Error(`backend query failed: ${control.event} ${control.message ?? ""}`);
  }
  return text.trim();
}

/** Sleep helper. */
export function sleep(ms) {
  return new Promise((r) => setTimeout(r, ms));
}
