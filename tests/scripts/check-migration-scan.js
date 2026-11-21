/* run migration and recovery in distinct processes against one isolated catalog.
a process-local flag would pass same-process restart tests and fail this contract. */
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const { mkdirSync, mkdtempSync, rmSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable, indexer, build] = process.argv.slice(2);
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'migration-scan-'));
const staged = stageTestRuntime(resolve(executable), root);
let passed = false;

try {
  let config = root;
  const stages = ['migration_prepare', 'migration_fail_completion'];
  if (indexer) {
    const seed = spawnSync(process.execPath, [
      join(__dirname, 'seed-test-library.js'), '--tracks', '3', '--indexer', indexer, '--fixture-dir', root, build,
    ], { encoding: 'utf8', timeout: 10000 });
    assert.ifError(seed.error);
    assert.equal(seed.status, 0, seed.stderr);
    const database = seed.stdout.trim().split(/\r?\n/).at(-1);
    config = resolve(database, '../../..');
    stages.push('migration_fail_metadata');
  }

  /* Starting the indexer must recover the work committed by the library-only process. */
  const env = { ...process.env, XDG_CONFIG_HOME: config, APPDATA: config };
  for (const mode of [...stages, 'migration_resume', 'migration_verify']) {
    const result = spawnSync(staged, [mode], { env, stdio: 'inherit', timeout: 10000 });
    assert.ifError(result.error);
    assert.equal(result.status, 0, `${mode} failed (${result.signal ?? 'exit'})`);
  }
  passed = true;
} finally {
  /* retain databases and provider state when any phase fails for diagnosis. */
  if (passed) {
    rmSync(root, { recursive: true });
  } else {
    console.error(`Migration scan fixture retained at ${root}`);
  }
}
