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

/** Build the WS URL for a given output format and options ({ logs, path }). */
export function urlFor(format = "JSONEachRow", { logs = "", path = "/" } = {}) {
  const params = new URLSearchParams({ format });
  if (logs) params.set("logs", logs);
  return `${PROXY_URL}${path}?${params}`;
}

/**
 * A single proxy session over one WebSocket. Supports multiple sequential
 * queries. Incoming frames are buffered into an async queue so callers can
 * `await` them regardless of arrival timing.
 */
export class Session {
  constructor(format = "JSONEachRow", { logs = "", path = "/" } = {}) {
    this.ws = new WebSocket(urlFor(format, { logs, path }));
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

  /** Next incoming frame: {type:'text',data:string} | {type:'binary',data:Buffer} | {type:'close'}. */
  nextFrame() {
    if (this._queue.length) return Promise.resolve(this._queue.shift());
    return new Promise((resolve) => this._waiters.push(resolve));
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
    const done = (control) => {
      const data = Buffer.concat(chunks);
      return { data, text: data.toString("utf8"), progress, logs, profileEvents, control };
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
   * Run an INSERT: send the query, stream the data chunks, end, and collect
   * the terminal control event. `chunks` is an array of strings/Buffers.
   */
  async insert(query, chunks = []) {
    await this.ready();
    this.sendQuery(query);
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
export async function runQuery(sql, { format = "JSONEachRow", logs = "" } = {}) {
  const s = new Session(format, { logs });
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
