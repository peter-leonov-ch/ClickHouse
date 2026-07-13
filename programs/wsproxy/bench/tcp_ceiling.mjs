import net from "node:net";
const BYTES = 962_000_000;
const buf = Buffer.allocUnsafe(1 << 20); // 1MB chunks
const server = net.createServer((sock) => {
  let sent = 0;
  const pump = () => {
    while (sent < BYTES) {
      sent += buf.length;
      if (!sock.write(buf)) { sock.once("drain", pump); return; }
    }
    sock.end();
  };
  pump();
});
server.listen(0, "127.0.0.1", () => {
  const port = server.address().port;
  let got = 0, t0 = performance.now();
  const c = net.connect(port, "127.0.0.1");
  c.on("data", (d) => { got += d.length; });
  c.on("end", () => {
    const ms = performance.now() - t0;
    console.log(`raw TCP recv: ${(got/1e6).toFixed(0)}MB in ${ms.toFixed(0)}ms = ${(got/1e6/(ms/1000)).toFixed(0)} MB/s`);
    server.close();
  });
});
