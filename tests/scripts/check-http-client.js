/* exercise the shared HTTP interface against a local server. */
const http = require('node:http');
const https = require('node:https');
const { readFileSync } = require('node:fs');
const { spawn } = require('node:child_process');
const { once } = require('node:events');

async function run(protocol) {
  const handler = (request, response) => {
    if (request.url === '/cancel-active') {
      active.stdin.end('cancel\n');
      return;
    }
    if (request.url === '/timeout') return;
    if (request.url === '/redirect') {
      response.writeHead(302, { Location: '/get' }).end();
      return;
    }
    if (request.url === '/error') {
      response.writeHead(404).end();
      return;
    }
    if (request.url === '/get') {
      response.writeHead(200, { 'X-Reply': 'present' }).end('world');
      return;
    }
    let body = '';
    request.on('data', data => { body += data; });
    request.on('end', () => {
      if (request.method !== 'POST' || request.headers['x-test'] !== 'yes' || body !== 'hello') {
        response.writeHead(400).end();
      } else {
        response.writeHead(200, { 'X-Reply': 'present' }).end('world');
      }
    });
  };
  const server = protocol === 'https'
    ? https.createServer({ cert: readFileSync(process.argv[3]), key: readFileSync(process.argv[4]) }, handler)
    : http.createServer(handler);
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  try {
    for (const mode of protocol === 'https' ? ['post', 'verified', 'untrusted-ip'] :
      ['post', 'get', 'redirect', 'error', 'background', 'cancel', 'cancel-active', 'timeout']) {
      const child = spawn(process.argv[2], [String(server.address().port), mode, protocol], { stdio: ['pipe', 'inherit', 'inherit'] });
      active = child;
      const timer = setTimeout(() => child.kill(), 5000);
      const [status] = await once(child, 'exit');
      clearTimeout(timer);
      if (status !== 0) throw new Error(`${mode} failed: ${status}`);
    }
  } finally {
    server.closeAllConnections();
    server.close();
  }
}

let active;
async function main() {
  await run('http');
  await run('https');
}
main().catch(error => { console.error(error); process.exitCode = 1; });
