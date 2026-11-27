/* Serve the native PCM fixtures with a delayed response for the replacement. */
const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const { spawn } = require('node:child_process');

const [parent, executable, capture, httpPlugin, mode] = process.argv.slice(2);
fs.mkdirSync(parent, { recursive: true });
const refillDelayMs = Number(process.env.MUSIKCORE_TEST_REFILL_DELAY_MS ?? 120);
const media = fs.mkdtempSync(path.join(parent, 'http-pcm-'));
const server = http.createServer((request, response) => {
  if (!['/first.wav', '/second.wav'].includes(request.url)) {
    response.writeHead(404).end();
    return;
  }
  const file = path.join(media, request.url.slice(1));
  const size = fs.statSync(file).size;
  const range = /^bytes=(\d+)-(\d*)$/.exec(request.headers.range || '');
  const start = range ? Number(range[1]) : 0;
  const end = range && range[2] ? Math.min(Number(range[2]), size - 1) : size - 1;
  if (start > end) {
    response.writeHead(416, { 'Content-Range': 'bytes */' + size }).end();
    return;
  }
  setTimeout(() => {
    if (response.destroyed) return;
    const headers = { 'Accept-Ranges': 'bytes', 'Content-Type': 'audio/wav', 'Content-Length': end - start + 1 };
    if (range) headers['Content-Range'] = 'bytes ' + start + '-' + end + '/' + size;
    response.writeHead(range ? 206 : 200, headers);
    if (request.method === 'HEAD') response.end();
    else {
      const stream = fs.createReadStream(file, { start, end });
      response.on('close', () => stream.destroy());
      if (request.url === '/second.wav') {
        // Exercise refill timing after the first music bytes have arrived.
        let delivered = start;
        const delayRefill = chunk => {
          delivered += chunk.length;
          if (delivered < 320 * 1024) return;
          stream.off('data', delayRefill);
          stream.pause();
          setTimeout(() => stream.resume(), refillDelayMs);
        };
        stream.on('data', delayRefill);
      }
      stream.pipe(response);
    }
  }, request.url === '/second.wav' ? 350 : 0);
});
server.listen(0, '127.0.0.1', () => {
  const child = spawn(process.execPath, [
    path.join(__dirname, 'run-isolated-test.js'), parent, executable, '--stage',
    '--plugin', capture, '--plugin', httpPlugin, mode, path.basename(capture), media
  ], { stdio: 'inherit', env: {
    ...process.env, MUSIKCORE_PCM_TEST_SERVER: 'http://127.0.0.1:' + server.address().port
  } });
  child.on('exit', code => {
    process.exitCode = code ?? 1;
    server.close();
    server.closeAllConnections();
    if (code === 0) fs.rmSync(media, { recursive: true });
  });
});
