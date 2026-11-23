/* two independent engines download the same URI into the same data directory.
finish the first while the second has only its prefix, then kill the second.
a fresh engine must replay the complete response and clean the abandoned file. */
const assert = require('node:assert/strict');
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { copyFileSync, mkdtempSync, readdirSync, rmSync } = require('node:fs');
const { createServer } = require('node:http');
const { basename, dirname, join } = require('node:path');

async function main() {
  const [executable, plugin] = process.argv.slice(2);
  const root = mkdtempSync(join(dirname(dirname(plugin)), 'http-cache-'));
  const data = join(root, 'data');
  copyFileSync(plugin, join(root, basename(plugin)));

  const children = [];
  const responses = [];
  const server = createServer((request, response) => {
    responses.push(response);
    if (responses.length > 2) {
      response.writeHead(404).end();
      return;
    }
    response.writeHead(200, { 'Content-Type': 'application/octet-stream' });
    response.write(Buffer.alloc(32768, responses.length === 1 ? 'A' : 'B'));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const url = `http://127.0.0.1:${server.address().port}/one-track`;

  /* each client signals when it has read the prefix. waiting for that signal
  lets the test overlap both transfers without relying on a timer. */
  function start(expected) {
    const child = spawn(executable, ['--cache-client', root, data, url, expected],
      { stdio: ['ignore', 'pipe', 'inherit'], timeout: 10000, killSignal: 'SIGKILL' });
    const done = once(child, 'exit');
    children.push({ child, done });

    const ready = Promise.race([
      once(child.stdout, 'data').then(([text]) => assert.equal(text.toString().trim(), 'ready')),
      done.then(() => { throw new Error('client exited before publishing its first chunk'); }),
    ]);
    return { child, ready, done };
  }

  try {
    const first = start('A');
    await first.ready;
    const second = start('B');
    await second.ready;

    responses[0].end('A');
    assert.equal((await first.done)[0], 0);
    second.child.kill('SIGKILL');
    await second.done;

    const replay = start('A');
    await replay.ready;
    assert.equal((await replay.done)[0], 0);
    assert.equal(responses.length, 2);
    assert.equal(readdirSync(join(data, 'cache', 'httpclient')).length, 1);
  } finally {
    for (const { child } of children) {
      if (child.exitCode === null && !child.signalCode) {
        child.kill('SIGKILL');
      }
    }
    await Promise.allSettled(children.map(client => client.done));
    server.closeAllConnections();
    server.close();
    rmSync(root, { recursive: true, force: true });
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
