/* exercise a real WebSocket handshake over a local TLS server. */
const { spawn } = require('node:child_process');
const { createHash } = require('node:crypto');
const { readFileSync } = require('node:fs');
const https = require('node:https');
const net = require('node:net');
const { once } = require('node:events');

const [client, cert, key] = process.argv.slice(2);

async function run(server, mode) {
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const child = spawn(client, [String(server.address().port), mode], { stdio: 'inherit' });
  const timer = setTimeout(() => child.kill(), 10000);
  const [status] = await once(child, 'exit');
  clearTimeout(timer);
  server.close();
  if (status !== 0) throw new Error(`${mode} client failed: ${status}`);
}

async function main() {
  const server = https.createServer({ cert: readFileSync(cert), key: readFileSync(key) });
  server.on('upgrade', (request, socket) => {
    socket.on('error', () => {});
    const accept = createHash('sha1').update(request.headers['sec-websocket-key'] +
      '258EAFA5-E914-47DA-95CA-C5AB0DC85B11').digest('base64');
    socket.write(`HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ${accept}\r\n\r\n`);
    let input = Buffer.alloc(0);
    socket.on('data', chunk => {
      input = Buffer.concat([input, chunk]);
      if (input.length < 11) return;
      if (input[0] !== 0x81 || input[1] !== 0x85) {
        socket.destroy(new Error('invalid client frame'));
        return;
      }
      const payload = Buffer.from(input.subarray(6, 11));
      for (let i = 0; i < payload.length; ++i) payload[i] ^= input[2 + i % 4];
      if (payload.toString() !== 'hello') {
        socket.destroy(new Error('unexpected client payload'));
        return;
      }
      socket.write(Buffer.from([0x81, 5, ...Buffer.from('world')]));
    });
  });
  await run(server, 'success');
  await run(server, 'cancel');
  const invalid = net.createServer(socket => socket.end('not TLS'));
  await run(invalid, 'fail');
}

main().catch(error => { console.error(error); process.exitCode = 1; });
