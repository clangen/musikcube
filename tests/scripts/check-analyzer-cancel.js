/* stop an indexer during a blocked stream open and during analyzer finish.
each run owns its catalog and synchronizes with the exact blocked callback. */
const assert = require('node:assert/strict');
const { spawn, spawnSync } = require('node:child_process');
const { copyFileSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFile, writeFileSync } = require('node:fs');
const { basename, dirname, join } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

const [seeder, analyzer, streamPlugin, checkMetadata] = process.argv.slice(2);

function findDatabase(directory) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      const found = findDatabase(path);
      if (found) {
        return found;
      }
    } else if (entry.name === 'musik.db') {
      return path;
    }
  }
}

async function run(action, root) {
  const directory = join(root, action);
  const media = join(directory, 'media');
  mkdirSync(media, { recursive: true });

  const wav = Buffer.alloc(44 + 16000);
  wav.write('RIFF', 0);
  wav.writeUInt32LE(wav.length - 8, 4);
  wav.write('WAVEfmt ', 8);
  wav.writeUInt32LE(16, 16);
  wav.writeUInt16LE(1, 20);
  wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(8000, 24);
  wav.writeUInt32LE(16000, 28);
  wav.writeUInt16LE(2, 32);
  wav.writeUInt16LE(16, 34);
  wav.write('data', 36);
  wav.writeUInt32LE(16000, 40);
  writeFileSync(join(media, 'analysis-block.wav'), wav);

  const executable = stageTestRuntime(seeder, directory);
  const gated = action === 'finish' || action === 'artwork';
  const plugins = gated ? [analyzer] : [analyzer, streamPlugin];
  for (const plugin of plugins) {
    copyFileSync(plugin, join(directory, 'runtime', 'plugins', basename(plugin)));
  }

  const gate = join(directory, 'finish-gate');
  if (gated) {
    assert.equal(spawnSync('mkfifo', [gate]).status, 0, 'create finish gate');
  }
  const args = action === 'artwork' ? [media, '1'] : [media, '1', action === 'finish' ? 'cancel' : action];
  const child = spawn(executable, args, {
    env: {
      ...process.env,
      XDG_CONFIG_HOME: directory,
      APPDATA: directory,
      ...(gated ? { MUSIKCUBE_ANALYZER_FINISH_GATE: gate } : {}),
      ...(action === 'artwork' ? { MUSIKCUBE_ANALYZER_WRITE_ARTWORK: '1' } : {}),
    },
    stdio: ['pipe', 'pipe', 'pipe'],
  });

  let output = '';
  let errors = '';
  let released = false;
  let gateReleased = false;
  child.stderr.on('data', chunk => { errors += chunk; });
  child.stdout.on('data', chunk => {
    output += chunk;
    const signal = gated ? 'analysis-finish\n' : 'analysis-open\n';
    if (!released && output.includes(signal)) {
      released = true;
      if (action === 'artwork') {
        const database = findDatabase(join(directory, 'musikcube'));
        const thumbs = join(dirname(database), 'thumbs');
        rmSync(thumbs, { recursive: true, force: true });
        writeFileSync(thumbs, 'not a directory');
        gateReleased = true;
        writeFile(gate, 'x', error => {
          if (error) {
            child.kill('SIGKILL');
          }
        });
      } else {
        child.stdin.end('\n');
      }
    }
    if (action === 'finish' && !gateReleased && output.includes('cancel-sent\n')) {
      gateReleased = true;
      writeFile(gate, 'x', error => {
        if (error) {
          child.kill('SIGKILL');
        }
      });
    }
  });

  const result = await new Promise((resolve, reject) => {
    const timer = setTimeout(() => {
      child.kill('SIGKILL');
      reject(new Error(`${action} timed out while analysis was blocked`));
    }, 15000);
    child.on('error', error => { clearTimeout(timer); reject(error); });
    child.on('exit', (code, signal) => {
      clearTimeout(timer);
      resolve({ code, signal });
    });
  });
  assert.ok(released, 'the analyzer must reach its blocked operation');
  assert.equal(result.code, 0, `${action} failed (${result.signal || result.code}): ${errors}`);
  if (action === 'finish') {
    const database = findDatabase(join(directory, 'musikcube'));
    assert.ok(database, 'indexed library database');
    const checked = spawnSync(checkMetadata, [database, '0'], { encoding: 'utf8' });
    assert.equal(checked.status, 0, checked.stderr || 'cancelled analysis was saved');
  }
  if (action === 'artwork') {
    const database = findDatabase(join(directory, 'musikcube'));
    const checked = spawnSync(checkMetadata, [database, '1', '0'], { encoding: 'utf8' });
    assert.equal(checked.status, 0, checked.stderr || 'failed artwork left a thumbnail record');
  }
}

async function main() {
  const root = mkdtempSync(join(dirname(seeder), 'analyzer-cancel-'));
  try {
    await run('cancel', root);
    await run('shutdown', root);
    /* TODO: add a Windows synchronization gate for cancellation during finish. */
    if (process.platform !== 'win32') {
      await run('finish', root);
      await run('artwork', root);
    }
  } finally {
    rmSync(root, { recursive: true, force: true });
  }
}

main().catch(error => { console.error(error); process.exitCode = 1; });
