/* Coordinate a withheld remote reply with the installed consumer's catalog switch. */
const { spawn } = require('node:child_process');
const { once } = require('node:events');
const { createInterface } = require('node:readline');

const [serverPath, clientPath, catalog] = process.argv.slice(2);

async function run() {
  const children = new Set();
  const start = (executable, args) => {
    const child = spawn(executable, args, { stdio: ['pipe', 'pipe', 'inherit'] });
    children.add(child);
    const closed = once(child, 'close').then(([status, signal]) => {
      children.delete(child);
      if (status !== 0) throw new Error(`${executable} exited with ${signal || status}`);
    });
    /* Observe process failures even while waiting for protocol output. */
    closed.catch(() => {});
    return { child, closed };
  };

  const server = start(serverPath, ['--serve-installed']);
  const serverLines = createInterface({ input: server.child.stdout });
  const timer = setTimeout(() => {
    console.error('Installed session fixture timed out');
    for (const child of children) child.kill('SIGKILL');
  }, 20000);

  try {
    const port = await Promise.race([
      once(serverLines, 'line').then(([line]) => line),
      server.closed.then(() => { throw new Error('Loopback fixture exited before publishing its port'); }),
    ]);
    if (!/^\d+$/.test(port)) throw new Error('Invalid loopback port: ' + port);

    const client = start(clientPath, [catalog, port]);
    client.child.stdin.end();
    const clientLines = createInterface({ input: client.child.stdout });
    server.child.stdin.on('error', error => console.error(error.message));
    clientLines.on('line', line => {
      if (line === 'release') {
        server.child.stdin.write('release\n');
      } else {
        console.log(line);
      }
    });

    try {
      await client.closed;
    } finally {
      server.child.stdin.end();
    }
    await server.closed;
  } finally {
    clearTimeout(timer);
    for (const child of children) child.kill();
  }
}

run().catch(error => {
  console.error(error.message);
  process.exitCode = 1;
});
