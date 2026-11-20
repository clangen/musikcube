const { spawnSync } = require('child_process');
const { resolve, join } = require('path');
const { parseArgs } = require('util');
const source = resolve(__dirname, '../..');

function run(args) {
  const result = spawnSync(process.execPath, args, { cwd: source, stdio: 'inherit' });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`Baseline command failed: ${result.signal || result.status}`);
}

try {
  const { values } = parseArgs({ options: {
    'build-dir': { type: 'string', default: join(source, 'build/core-baseline') },
    config: { type: 'string', default: 'Release' },
    help: { type: 'boolean' },
  } });
  if (values.help) {
    console.log('Usage: node script/test/run-baseline.js [--build-dir <dir>] [--config <name>]');
  } else {
    const build = resolve(values['build-dir']);
    run([join(__dirname, 'run-tests.js'), '--build-dir', build, '--config', values.config, '--abi-only']);
    const binary = process.platform === 'win32'
      ? join(build, 'bin/abi0', values.config, 'musikcore_api_baseline.exe')
      : join(build, 'bin/abi0/musikcore_api_baseline');
    run([join(__dirname, 'run-isolated-test.js'), join(build, 'tests/bin/baseline'), binary, '@origin']);
  }
} catch (error) { console.error(error.message); process.exitCode = 1; }
