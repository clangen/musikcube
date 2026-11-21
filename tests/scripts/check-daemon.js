/* Exercise the installed-style daemon with an isolated catalog and lockfile. */
const assert = require('node:assert/strict');
const { once } = require('node:events');
const { spawn, execFile } = require('node:child_process');
const { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { promisify } = require('node:util');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable] = process.argv.slice(2);
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'daemon-'));
const daemon = stageTestRuntime(resolve(executable), root);
const lockfile = join(root, 'daemon.lock');
const env = { ...process.env, XDG_CONFIG_HOME: root, MUSIKCUBED_LOCKFILE_OVERRIDE: lockfile };

function waitFor(child, stream, marker) {
  return new Promise((resolve, reject) => {
    let output = '';

    function onExit(code, signal) {
      stream.off('data', onData);
      reject(new Error(`Daemon exited before ${marker} (${code}, ${signal}): ${output}`));
    }

    function onData(chunk) {
      output += chunk;
      if (output.includes(marker)) {
        stream.off('data', onData);
        child.off('exit', onExit);
        resolve();
      }
    }

    child.once('exit', onExit);
    stream.on('data', onData);
  });
}

function run(arg) {
  return promisify(execFile)(daemon, [arg], { env, timeout: 7000 });
}

async function check() {
  const children = Array.from({ length: 4 }, () => {
    const child = spawn(daemon, ['--foreground'], { env, stdio: ['ignore', 'pipe', 'pipe'] });
    return { child, exited: once(child, 'exit') };
  });
  const deadline = setTimeout(() => children.forEach(({ child }) => child.kill('SIGKILL')), 15000);
  try {
    const winner = await Promise.any(children.map(async entry => {
      await waitFor(entry.child, entry.child.stdout, 'musikcubed ready');
      return entry;
    }));
    for (const entry of children) {
      if (entry !== winner) {
        assert.deepEqual(await entry.exited, [0, null]);
      }
    }
    assert.equal(Number(readFileSync(lockfile, 'utf8')), winner.child.pid);
    assert.match((await run('--running')).stdout, /is running with pid/);

    /* Drain the startup scan before requesting and observing a second scan. */
    await waitFor(winner.child, winner.child.stderr, 'Indexer: done!');
    const rescanned = waitFor(winner.child, winner.child.stderr, 'Indexer: done!');
    assert(winner.child.kill('SIGUSR1'));
    await rescanned;

    assert(winner.child.kill('SIGTERM'));
    assert.deepEqual(await winner.exited, [0, null]);
    assert(!existsSync(lockfile));

    /* Background startup must return failure when the child cannot publish its PID. */
    const blocked = lockfile + '.pid';
    mkdirSync(blocked);
    await assert.rejects(run('--start'), { code: 1 });
    assert(!existsSync(lockfile));
    rmSync(blocked, { recursive: true });

    await run('--start');
    assert(existsSync(lockfile));
    assert.match((await run('--running')).stdout, /is running with pid/);
    await run('--stop');
    assert(!existsSync(lockfile));
    rmSync(root, { recursive: true });
  }
  catch (error) {
    children.forEach(({ child }) => child.kill('SIGKILL'));
    await run('--stop').catch(() => {});
    console.error(`Daemon fixture retained at ${root}`);
    throw error;
  }
  finally {
    clearTimeout(deadline);
  }
}

check().catch(error => {
  console.error(error);
  process.exitCode = 1;
});
