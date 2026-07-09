// A raw-TCP WebSocket client for adversarial tests. Unlike the native global
// WebSocket, this lets us craft malformed frames (unmasked, reserved opcodes,
// oversized advertised lengths, bad fragmentation) to exercise the proxy's RFC
// 6455 hardening.

import net from "node:net";
import crypto from "node:crypto";

const GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

export class RawClient {
  constructor({ host = "127.0.0.1", port = 9010 } = {}) {
    this.socket = net.connect({ host, port });
    this.socket.setNoDelay(true);
    this.buf = Buffer.alloc(0);
    this.closed = false;
    this._wake = null;
    this.socket.on("data", (chunk) => {
      this.buf = Buffer.concat([this.buf, chunk]);
      this._signal();
    });
    this.socket.on("close", () => {
      this.closed = true;
      this._signal();
    });
    this.socket.on("error", () => {
      this.closed = true;
      this._signal();
    });
  }

  _signal() {
    if (this._wake) {
      const w = this._wake;
      this._wake = null;
      w();
    }
  }

  _waitData() {
    return new Promise((resolve) => (this._wake = resolve));
  }

  connected() {
    return new Promise((resolve, reject) => {
      this.socket.once("connect", resolve);
      this.socket.once("error", reject);
    });
  }

  /** Perform the WebSocket upgrade handshake; resolves when the 101 is received. */
  async handshake(path = "/?format=JSONEachRow") {
    await this.connected();
    const key = crypto.randomBytes(16).toString("base64");
    this.socket.write(
      `GET ${path} HTTP/1.1\r\nHost: 127.0.0.1\r\n` +
        "Upgrade: websocket\r\nConnection: Upgrade\r\n" +
        `Sec-WebSocket-Key: ${key}\r\nSec-WebSocket-Version: 13\r\n\r\n`,
    );
    while (this.buf.indexOf("\r\n\r\n") === -1 && !this.closed) await this._waitData();
    const idx = this.buf.indexOf("\r\n\r\n");
    if (idx === -1) throw new Error("connection closed during handshake");
    const head = this.buf.subarray(0, idx).toString("latin1");
    this.buf = this.buf.subarray(idx + 4);
    if (!head.includes("101 Switching Protocols")) throw new Error(`no 101:\n${head}`);
    const expect = crypto.createHash("sha1").update(key + GUID).digest("base64");
    if (!head.includes(`Sec-WebSocket-Accept: ${expect}`)) throw new Error("bad accept");
    return head;
  }

  /**
   * Send a WebSocket frame with full control over the header.
   * opts: { opcode, payload (Buffer|string), fin=true, masked=true, rsv=0,
   *         advertisedLen (override the length field to lie about payload size) }
   */
  sendFrame({ opcode, payload = Buffer.alloc(0), fin = true, masked = true, rsv = 0, advertisedLen }) {
    if (typeof payload === "string") payload = Buffer.from(payload);
    const len = advertisedLen ?? payload.length;
    const header = [];
    header.push((fin ? 0x80 : 0) | (rsv << 4) | (opcode & 0x0f));
    let ext = Buffer.alloc(0);
    const maskBit = masked ? 0x80 : 0;
    if (len < 126) {
      header.push(maskBit | len);
    } else if (len < 65536) {
      header.push(maskBit | 126);
      ext = Buffer.alloc(2);
      ext.writeUInt16BE(len);
    } else {
      header.push(maskBit | 127);
      ext = Buffer.alloc(8);
      ext.writeBigUInt64BE(BigInt(len));
    }
    let body = payload;
    let maskKey = Buffer.alloc(0);
    if (masked) {
      maskKey = crypto.randomBytes(4);
      body = Buffer.from(payload);
      for (let i = 0; i < body.length; i++) body[i] ^= maskKey[i % 4];
    }
    this.socket.write(Buffer.concat([Buffer.from(header), ext, maskKey, body]));
  }

  /** Read one server->client frame, or {closed:true} if the socket closes first. */
  async readFrame() {
    const need = async (n) => {
      while (this.buf.length < n && !this.closed) await this._waitData();
      return this.buf.length >= n;
    };
    if (!(await need(2))) return { closed: true };
    const b0 = this.buf[0];
    const b1 = this.buf[1];
    const opcode = b0 & 0x0f;
    const fin = (b0 & 0x80) !== 0;
    let len = b1 & 0x7f;
    let offset = 2;
    if (len === 126) {
      if (!(await need(4))) return { closed: true };
      len = this.buf.readUInt16BE(2);
      offset = 4;
    } else if (len === 127) {
      if (!(await need(10))) return { closed: true };
      len = Number(this.buf.readBigUInt64BE(2));
      offset = 10;
    }
    if (!(await need(offset + len))) return { closed: true };
    const payload = this.buf.subarray(offset, offset + len);
    this.buf = this.buf.subarray(offset + len);
    return { opcode, fin, payload: Buffer.from(payload) };
  }

  /** Resolve once the socket has closed (with a timeout guard). */
  async waitClose(timeoutMs = 5000) {
    const deadline = Date.now() + timeoutMs;
    while (!this.closed && Date.now() < deadline) {
      await Promise.race([this._waitData(), new Promise((r) => setTimeout(r, 200))]);
    }
    return this.closed;
  }

  close() {
    try {
      this.socket.destroy();
    } catch {
      /* ignore */
    }
  }
}
