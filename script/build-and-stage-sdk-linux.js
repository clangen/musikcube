const { spawnSync } = require('node:child_process');
const { existsSync, mkdirSync, realpathSync, rmSync } = require('node:fs');
const { dirname, join, resolve } = require('node:path');
const { buildArchiveConfiguration, configureZigArchive } = require('./unix-build-config');

const source = resolve(__dirname, '..');
const root = dirname(source);
const build = process.env.MUSIKCORE_SDK_BUILD;
const output = process.env.MUSIKCORE_SDK_OUTPUT;

function run(program, args, env, capture = false, cwd = source) {
  const result = spawnSync(program, args, {
    cwd, env, encoding: 'utf8', stdio: capture ? ['ignore', 'pipe', 'inherit'] : 'inherit',
  });
  if (result.error || result.status !== 0) {
    throw result.error || new Error(`${program} failed (${result.signal || result.status})`);
  }
  return capture ? result.stdout.trim() : undefined;
}

async function main() {
  if (process.arch !== 'x64') throw new Error('The Linux SDK build requires x86-64.');
  const vendor = join(root, 'vendor-x86_64-zig-glibc2.28');
  const link = join(source, 'vendor');
  if (!existsSync(vendor) || !existsSync(link) || realpathSync(link) !== realpathSync(vendor)) {
    throw new Error(`The musikcube vendor link must point to ${vendor}. Build the Zig vendors first.`);
  }
  const config = buildArchiveConfiguration(root, 'zig');
  await configureZigArchive(config, { root, vendor, run });
  mkdirSync(config.env.TMPDIR, { recursive: true });
  if (process.env.MUSIKCORE_SDK_CLEAN === '1') {
    if (existsSync(join(build, 'CMakeCache.txt'))) {
      run('cmake', ['--build', build, '--target', 'clean'], config.env);
    }
    rmSync(join(build, 'bin'), { recursive: true, force: true });
  }
  run('bash', [join(source, 'script/stage-vendor-libraries.sh'), vendor], config.env, false, build);
  run('cmake', ['-S', source, '-B', build, ...config.cmake, '-DCMAKE_BUILD_TYPE=Release',
    '-DBUILD_STANDALONE=true', '-DBUILD_TESTING=OFF', `-DMUSIKCUBE_VENDOR_ROOT=${vendor}`,
    `-DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY=${join(build, 'bin')}`], config.env);
  run('cmake', ['--build', build, '--parallel', '8', '--target', 'musikcube', 'musikcubed'], config.env);
  run('sh', [join(source, 'script/patch-rpath.sh'), source], config.env, false, build);
  for (const component of ['musikcoreSDK', 'musikcorePlugins']) {
    run('cmake', ['--install', build, '--prefix', output, '--component', component], config.env);
  }
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
