const { spawnSync } = require('child_process');
const { resolve } = require('path');
const { parseArgs } = require('util');

const source = resolve(__dirname, '../..');

function run(command, args, cwd) {
  const result = spawnSync(command, args, { cwd, stdio: 'inherit' });
  if (result.error) throw result.error;
  if (result.signal) process.kill(process.pid, result.signal);
  if (result.status !== 0) process.exit(result.status ?? 1);
}

function main() {
  const { values, positionals } = parseArgs({
    options: {
      'build-dir': { type: 'string' },
      config: { type: 'string', default: 'Debug' },
      'abi-only': { type: 'boolean', default: false },
      help: { type: 'boolean', short: 'h' },
    },
    allowPositionals: true,
  });

  if (values.help) {
    console.log('Usage: node script/test/run-tests.js [--build-dir <path>] [--config <name>] [--abi-only] [-- <ctest args>]');
    return;
  }

  const build = values['build-dir'] === undefined ? source : resolve(values['build-dir']);
  run('cmake', ['-S', source, '-B', build, '-DBUILD_TESTING=ON',
    `-DCMAKE_BUILD_TYPE=${values.config}`, `-DMUSIKCORE_ABI_ONLY=${values['abi-only'] ? 'ON' : 'OFF'}`], source);
  run('cmake', ['--build', build, '--parallel', '--config', values.config, '--target', 'musikcube_tests'], source);
  run('ctest', ['-C', values.config, '--output-on-failure', ...positionals], build);
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
