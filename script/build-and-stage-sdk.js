/* Build and stage the SDK and its runtime plugins for the host platform. */
const { spawnSync } = require('node:child_process');
const { cpSync, existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } = require('node:fs');
const { dirname, join, resolve, relative, isAbsolute } = require('node:path');

const root = resolve(__dirname, '..');
const args = process.argv.slice(2);
let output = join(root, 'build', 'sdk');
let clean = false;
let outputSpecified = false;
if (args.length === 1 && (args[0] === '--help' || args[0] === '-h')) {
  console.log('Usage: node script/build-and-stage-sdk.js [--output=<directory>] [--clean]');
  process.exit(0);
}
for (const arg of args) {
  if (arg === '--clean' && !clean) {
    clean = true;
  } else if (arg.startsWith('--output=') && arg.slice(9) && !outputSpecified) {
    output = resolve(arg.slice(9));
    outputSpecified = true;
  } else {
    console.error('Usage: node script/build-and-stage-sdk.js [--output=<directory>] [--clean]');
    process.exit(1);
  }
}

const platform = { win32: 'windows', linux: 'linux', darwin: 'macos' }[process.platform];
if (!platform) {
  console.error(`Unsupported platform: ${process.platform}`);
  process.exit(1);
}

function run(command, parameters, options = {}) {
  const result = spawnSync(command, parameters, { cwd: root, encoding: 'utf8', ...options });
  if (result.error || result.status !== 0) {
    throw result.error || new Error(`${command} failed (${result.signal || result.status})`);
  }
  return result.stdout;
}

try {
  const rootFromOutput = relative(output, root);
  if (!rootFromOutput || (!rootFromOutput.startsWith('..') && !isAbsolute(rootFromOutput))) {
    throw new Error('SDK output cannot be the source directory or one of its parents.');
  }
  if (existsSync(output) && readdirSync(output).length &&
      !existsSync(join(output, 'include', 'musikcore', 'musikcore.h')) &&
      !existsSync(join(output, 'build-info.json'))) {
    throw new Error(`Refusing to replace an unrecognized SDK directory: ${output}`);
  }
  const build = join(root, 'build', `sdk-${platform}`);
  mkdirSync(build, { recursive: true });
  const stage = mkdtempSync(join(build, 'stage-'));
  try {
    run(process.execPath, [join(__dirname, `build-and-stage-sdk-${platform}.js`)], {
      stdio: 'inherit', env: { ...process.env, MUSIKCORE_SDK_BUILD: build, MUSIKCORE_SDK_OUTPUT: stage,
        MUSIKCORE_SDK_CLEAN: clean ? '1' : '0' },
    });
    const version = readFileSync(join(root, 'src/common/version.h'), 'utf8')
      .match(/^#define MUSIKCUBE_VERSION "([^"]+)"/m)?.[1];
    if (!version) throw new Error('Could not read the musikcore version');
    const manifest = {
      version,
      sourceCommit: run('git', ['rev-parse', 'HEAD']).trim(),
      sourceModified: !!run('git', ['status', '--porcelain']).trim(),
      platform: process.platform,
      architecture: process.arch,
      configuration: 'Release',
    };
    writeFileSync(join(stage, 'build-info.json'), `${JSON.stringify(manifest, null, 2)}\n`);

    /* Replace only SDK-owned paths so removed plugins and DLLs cannot linger. */
    const paths = ['bin', 'include/musikcore', 'lib/musikcore.lib', 'lib/cmake/musikcore',
      'share/musikcube', 'build-info.json'];
    mkdirSync(output, { recursive: true });
    for (const path of paths) {
      const destination = join(output, path);
      rmSync(destination, { recursive: true, force: true });
      const source = join(stage, path);
      if (existsSync(source)) {
        mkdirSync(dirname(destination), { recursive: true });
        cpSync(source, destination, { recursive: true, verbatimSymlinks: true });
      }
    }
    console.log(`SDK ${version} staged at ${output}`);
  } finally {
    rmSync(stage, { recursive: true, force: true });
  }
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
