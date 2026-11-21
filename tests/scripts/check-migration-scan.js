/* run migration and recovery in distinct processes against one isolated catalog.
a process-local flag would pass same-process restart tests and fail this contract. */
const assert = require('node:assert/strict');
const { spawnSync } = require('node:child_process');
const { mkdirSync, mkdtempSync, rmSync } = require('node:fs');
const { join, resolve } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable] = process.argv.slice(2);
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'migration-scan-'));
const staged = stageTestRuntime(resolve(executable), root);
const env = { ...process.env, XDG_CONFIG_HOME: root, APPDATA: root };
let passed = false;

try {
  /* no stage explicitly requests a scan: enabling Indexer must recover the work
  committed by the earlier library-only process, even after completion fails. */
  for (const mode of ['migration_prepare', 'migration_fail_completion', 'migration_resume', 'migration_verify']) {
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
