const { spawnSync } = require('node:child_process');
const { existsSync, rmSync } = require('node:fs');
const { join, resolve } = require('node:path');

const root = resolve(__dirname, '..');
const build = process.env.MUSIKCORE_SDK_BUILD;
const output = process.env.MUSIKCORE_SDK_OUTPUT;

function run(args) {
  const result = spawnSync('cmake', args, { cwd: root, stdio: 'inherit' });
  if (result.error || result.status !== 0) {
    throw result.error || new Error(`cmake failed (${result.status})`);
  }
}

if (process.env.MUSIKCORE_SDK_CLEAN === '1') {
  if (existsSync(join(build, 'CMakeCache.txt'))) {
    run(['--build', build, '--config', 'Release', '--target', 'clean']);
  }
  rmSync(join(build, 'runtime'), { recursive: true, force: true });
}
run(['--preset', 'windows-x64', '-S', root, '-B', build, '-DBUILD_TESTING=OFF',
  `-DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY=${join(build, 'runtime')}`]);
run(['--build', build, '--config', 'Release', '--target', 'musikcore', 'musikcube_runtime', '--parallel', '6']);
for (const component of ['musikcoreSDK', 'musikcorePlugins']) {
  run(['--install', build, '--config', 'Release', '--prefix', output, '--component', component]);
}
