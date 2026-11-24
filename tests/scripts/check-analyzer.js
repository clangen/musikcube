/* exercise the analyzer through the production indexer with a real WAV and
verify that a failed processor is finished without committing its results. */
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } = require('node:fs');
const { basename, dirname, join } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

const [seeder, plugin, failingPlugin, checkMetadata] = process.argv.slice(2);

/* the configuration directory contains the private catalog at a platform-specific path. */
function findDatabase(directory) {
  for (const entry of readdirSync(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) {
      const found = findDatabase(path);
      if (found) return found;
    } else if (entry.name === 'musik.db') {
      return path;
    }
  }
}
const root = mkdtempSync(join(dirname(seeder), 'analyzer-'));

try {
  for (const mode of ['success', 'process-failure', 'create-failure', 'null-create']) {
    const fail = mode !== 'success';
    const scenario = join(root, mode);
    const media = join(scenario, 'media');
    mkdirSync(media, { recursive: true });
    const frames = 8000 * 12;
    const wav = Buffer.alloc(44 + frames * 2);
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
    wav.writeUInt32LE(frames * 2, 40);
    writeFileSync(join(media, 'track.wav'), wav);

    const executable = stageTestRuntime(seeder, scenario);
    copyFileSync(plugin, join(scenario, 'runtime', 'plugins', basename(plugin)));
    if (fail) {
      copyFileSync(failingPlugin, join(scenario, 'runtime', 'plugins', basename(failingPlugin)));
    }
    const log = join(scenario, 'calls.log');
    const env = { ...process.env, APPDATA: scenario, XDG_CONFIG_HOME: scenario,
      MUSIKCUBE_ANALYZER_TEST_LOG: log,
      ...(mode === 'create-failure' ? { MUSIKCUBE_ANALYZER_FAIL_CREATE: '1' } : {}),
      ...(mode === 'null-create' ? { MUSIKCUBE_ANALYZER_NULL_CREATE: '1', MUSIKCORE_TEST_EXPECT_ABORT: '1' } : {}) };
    const result = spawnSync(executable, [media, '1'], { env, encoding: 'utf8', timeout: 60000 });
    if (mode === 'null-create') {
      assert.ok(result.signal === 'SIGABRT' || process.platform === 'win32' && result.status === 3,
        result.stderr || 'a factory reporting success without an instance must terminate');
      continue;
    }
    assert.equal(result.status, 0, result.stderr || result.error?.message);
    if (mode === 'create-failure') {
      assert.equal(existsSync(log), false, 'no analyzer should run after a factory failure');
      const database = findDatabase(join(scenario, 'musikcube'));
      const checked = spawnSync(checkMetadata, [database, '0'], { encoding: 'utf8' });
      assert.equal(checked.status, 0, checked.stderr || 'unexpected persisted analyzer metadata');
      continue;
    }
    const calls = readFileSync(log, 'utf8').trim().split('\n');
    assert.equal(calls.filter(call => call === 'begin').length, fail ? 2 : 1);
    assert.ok(calls.includes('process'));
    if (!fail) {
      assert.ok(calls.includes(`frames=${frames}`), 'the analyzer must receive the complete track');
    }
    assert.equal(calls.filter(call => call === 'commit').length, fail ? 0 : 1);
    assert.equal(calls.filter(call => call === 'cancel').length, fail ? 2 : 0);
    const database = findDatabase(join(scenario, 'musikcube'));
    assert.ok(database, 'indexed library database');
    const checked = spawnSync(checkMetadata, [database, fail ? '0' : '1'], { encoding: 'utf8' });
    assert.equal(checked.status, 0, checked.stderr || 'unexpected persisted analyzer metadata');
  }
} finally {
  rmSync(root, { recursive: true, force: true });
}
