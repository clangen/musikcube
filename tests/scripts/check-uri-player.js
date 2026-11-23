/* stage only the real decoder, headless output, and optional HTTP provider.
no catalog/indexer is involved. drive a dynamic loopback server for normal and
interrupted opens while the native client checks public API lifetime contracts. */
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createServer } = require('node:http');
const { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } = require('node:fs');
const { basename, dirname, join, resolve } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

/* a short deterministic stereo PCM WAV with enough duration for pause/seek.
its escaped filename exercises file URI conversion without user-owned media. */
function wave(seconds = 2) {
  const samples = 48000 * seconds;
  const data = Buffer.alloc(samples * 4);
  const header = Buffer.alloc(44);
  header.write('RIFF'); header.writeUInt32LE(36 + data.length, 4); header.write('WAVEfmt ', 8);
  header.writeUInt32LE(16, 16); header.writeUInt16LE(1, 20); header.writeUInt16LE(2, 22);
  header.writeUInt32LE(48000, 24); header.writeUInt32LE(192000, 28);
  header.writeUInt16LE(4, 32); header.writeUInt16LE(16, 34);
  header.write('data', 36); header.writeUInt32LE(data.length, 40);
  return Buffer.concat([header, data]);
}

/* own fixture processes, sockets, and files through success or failure. */
async function main() {
  const [parent, executable, output, httpPlugin] = process.argv.slice(2);
  mkdirSync(parent, { recursive: true });
  const root = mkdtempSync(join(resolve(parent), 'uri-player-'));
  const staged = stageTestRuntime(resolve(executable), root);
  for (const plugin of [output, httpPlugin].filter(Boolean)) {
    copyFileSync(plugin, join(dirname(staged), 'plugins', basename(plugin)));
  }
  const media = wave();
  const liveMedia = wave(16);
  const filename = join(root, '音楽 # 100%.wav');
  writeFileSync(filename, media);
  /* seed the same file existing installations use. both lookup and later
  edits must target this store when the provider is loaded through the C SDK. */
  mkdirSync(join(root, 'musikcube'), { recursive: true });
  writeFileSync(join(root, 'musikcube', 'plugin_httpidatastream.json'), JSON.stringify({ max_cache_files: 0 }));
  const cacheRequests = new Map();
  const shortWave = wave(0.125);
  let client;
  const server = createServer((request, response) => {
    /* two players can download the same uncached URL. hold the first partial
    response until Stop. the second player receives all bytes and caches them. */
    if (request.url === '/overlap.wav') {
      const attempt = (cacheRequests.get(request.url) || 0) + 1;
      cacheRequests.set(request.url, attempt);
      response.writeHead(200, { 'Content-Type': 'audio/wav', 'Transfer-Encoding': 'chunked' });
      if (attempt === 1) {
        response.write(shortWave.subarray(0, 128), () => client.stdin.write('/overlap.wav\n'));
      } else {
        response.end(shortWave);
      }
      return;
    }

    /* an interrupted chunked response lacks a Content-Length, but its missing
    chunk terminator is still a transfer error. retry supplies the whole file. */
    if (request.url === '/truncated.wav') {
      const attempt = (cacheRequests.get(request.url) || 0) + 1;
      cacheRequests.set(request.url, attempt);
      response.writeHead(200, { 'Content-Type': 'audio/wav', 'Transfer-Encoding': 'chunked' });
      if (attempt === 1) {
        response.write(shortWave.subarray(0, 128), () => response.destroy());
      } else {
        response.end(shortWave);
      }
      return;
    }

    if (request.url === '/cached' || request.url === '/uncached') {
      cacheRequests.set(request.url, (cacheRequests.get(request.url) || 0) + 1);
      response.writeHead(200, { 'Content-Type': 'Audio/Wav; rate=48000', 'Transfer-Encoding': 'chunked' });
      response.end(shortWave);
      return;
    }

    if (request.url === '/retry.wav') {
      response.writeHead(429, { 'Content-Length': 0 });
      /* Give the downloader time to enter the backoff after receiving the response. */
      response.end(() => setTimeout(() => {
        if (!client.stdin.destroyed) client.stdin.write('/retry.wav\n');
      }, 200));
      return;
    }

    const body = request.url === '/empty.wav' ? Buffer.alloc(0)
      : request.url === '/live.wav' ? liveMedia : media;
    const headers = { 'Content-Type': 'audio/wav' };
    /* ongoing streams have no advertised tail for a decoder to probe/seek. */
    if (request.url !== '/live.wav') headers['Content-Length'] = body.length;
    response.writeHead(200, headers);
    if (request.url.startsWith('/blocked')) {
      response.flushHeaders();
      client.stdin.write(request.url + '\n');
    } else if (request.url === '/live.wav') {
      /* provide enough for decoder probing/prebuffering while retaining a live
      tail. readiness must not depend on HTTP completion; close interrupts it. */
      response.write(body.subarray(0, 12 * 192000 + 44));
    } else {
      response.end(body);
    }
  });
  let passed = false;
  try {
    await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
    const args = [filename];
    if (httpPlugin) args.push(`http://127.0.0.1:${server.address().port}`);
    client = spawn(staged, args, {
      env: { ...process.env, XDG_CONFIG_HOME: root, APPDATA: root },
      stdio: ['pipe', 'inherit', 'inherit'],
    });
    const timer = setTimeout(() => client.kill('SIGKILL'), 30000);
    try {
      const [status, signal] = await once(client, 'close');
      assert.equal(status, 0, `URI player failed (${signal ?? 'exit'})`);
      if (httpPlugin) {
        assert.equal(cacheRequests.get('/uncached'), 2, 'saved zero-entry cache disables reuse');
        assert.equal(cacheRequests.get('/cached'), 1, 'a new stream sees the updated cache limit');
        assert.equal(cacheRequests.get('/truncated.wav'), 2, 'an incomplete response cannot become a cache hit');
        assert.equal(cacheRequests.get('/overlap.wav'), 2, 'cancelling a partial download preserved the other cache entry');
      }
    } finally {
      clearTimeout(timer);
    }
    passed = true;
  } finally {
    client?.kill();
    server.closeAllConnections();
    await new Promise(resolve => server.close(resolve));
    if (passed) rmSync(root, { recursive: true });
    else console.error(`URI player fixture retained at ${root}`);
  }
}
main().catch(error => { console.error(error); process.exitCode = 1; });
