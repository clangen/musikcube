/* build and stage the sibling musikcube SDK for the Rust bindings and example.
keep compiler selection in musikcube's shared profile and dependencies beside
its runtime so the Rust app does not need Homebrew libraries at launch. */
const { spawnSync } = require('child_process');
const { appendFileSync, copyFileSync, existsSync, lstatSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } = require('fs');
const { basename, dirname, isAbsolute, join, resolve } = require('path');

if (process.platform !== 'darwin') {
  throw new Error('This staging entry point is for native macOS. See README.md for Linux.');
}

const project = resolve(__dirname, '..');
const source = resolve(process.env.MUSIKCUBE_SOURCE || join(project, '../../musikcube'));
const build = join(source, 'build/egui-sdk-release');
const runtime = join(build, 'bin');
const sdk = join(project, 'sdk');
const log = join(project, 'build/sdk-stage.log');
const { buildArchiveConfiguration } = require(join(source, 'script/unix-build-config'));
const config = buildArchiveConfiguration(dirname(source));
mkdirSync(dirname(log), { recursive: true });
mkdirSync(config.env.TMPDIR, { recursive: true });
writeFileSync(log, '');

/* retain complete build diagnostics while keeping successful staging quiet. */
function run(program, args) {
  const result = spawnSync(program, args, {
    cwd: source, env: config.env, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024,
  });
  appendFileSync(log, `${program} ${args.join(' ')}\n${result.stdout || ''}${result.stderr || ''}`);
  if (result.error || result.status !== 0) {
    throw new Error(`${program} failed: ${result.error || result.stderr}\nSee ${log}`);
  }
  return result.stdout;
}

/* enumerate real runtime files; symlink aliases are not separate dylib images. */
function libraries(directory) {
  return readdirSync(directory).filter(name => name.endsWith('.dylib') && lstatSync(join(directory, name)).isFile())
    .map(name => join(directory, name));
}

console.log('Building standalone Release musikcore and plugins...');
if (process.env.MUSIKCORE_SDK_CLEAN === '1') {
  if (existsSync(join(build, 'CMakeCache.txt'))) {
    run('cmake', ['--build', build, '--target', 'clean']);
  }
  rmSync(runtime, { recursive: true, force: true });
}
run('cmake', ['-S', source, '-B', build, ...config.cmake,
  '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_STANDALONE=true', '-DBUILD_TESTING=OFF',
  `-DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY=${runtime}`]);
run('cmake', ['--build', build, '--parallel', '8', '--target', 'musikcube', 'musikcubed']);

const dependencies = join(runtime, 'lib');
rmSync(dependencies, { recursive: true, force: true });
mkdirSync(dependencies, { recursive: true });
const binaries = [join(runtime, 'musikcube'), join(runtime, 'musikcubed'),
  join(runtime, 'libmusikcore.dylib'), ...libraries(join(runtime, 'plugins'))];
const projectmHelper = join(runtime, 'plugins/musikcube-projectm');
if (existsSync(projectmHelper)) binaries.push(projectmHelper);
const cache = readFileSync(join(build, 'CMakeCache.txt'), 'utf8');
const portaudio = cache.match(/^LIBPORTAUDIO:FILEPATH=(.+)$/m)?.[1];

/* stage the actual dependency closure, avoiding unused vendor libraries and
symlink aliases that are not needed by this runtime. */
for (let index = 0; index < binaries.length; ++index) {
  const file = binaries[index];
  const needed = run('otool', ['-L', file]).split('\n').slice(1)
    .map(line => line.trim().split(' (compatibility version')[0]).filter(Boolean);
  for (const library of needed) {
    if (library.startsWith('/System/') || library.startsWith('/usr/lib/') ||
        basename(library) === basename(file)) {
      continue;
    }
    if (isAbsolute(library)) {
      run('install_name_tool', ['-change', library, `@rpath/${basename(library)}`, file]);
    }
    if (binaries.some(binary => basename(binary) === basename(library))) {
      continue;
    }
    let original = isAbsolute(library) ? library
      : join(source, 'vendor/bin/lib', basename(library));
    if (!existsSync(original) && portaudio && basename(library).startsWith('libportaudio.')) {
      original = portaudio;
    }
    if (!existsSync(original)) {
      throw new Error(`Cannot stage dependency ${library} of ${file}`);
    }
    const destination = join(dependencies, basename(library));
    copyFileSync(original, destination);
    binaries.push(destination);
  }
}

/* remove build-machine search paths before signing and auditing the runtime. */
for (const file of binaries) {
  if (dirname(file) === dependencies) {
    run('install_name_tool', ['-id', `@rpath/${basename(file)}`, file]);
  }
  const commands = run('otool', ['-l', file]);
  const rpaths = [...commands.matchAll(/cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset \d+\)/g)].map(match => match[1]);
  const desired = dirname(file) === dependencies ? ['@loader_path']
    : dirname(file) === join(runtime, 'plugins') ? ['@loader_path', '@loader_path/../lib', '@loader_path/..']
    : ['@loader_path', '@loader_path/lib'];
  for (const path of new Set(rpaths)) {
    if (!desired.includes(path)) {
      run('install_name_tool', ['-delete_rpath', path, file]);
    }
  }
  for (const path of desired) {
    if (!rpaths.includes(path)) {
      run('install_name_tool', ['-add_rpath', path, file]);
    }
  }
  run('codesign', ['--force', '--sign=-', file]);
}

console.log('Auditing runtime dependencies and installing SDK...');
run(process.execPath, [join(source, 'script/scan-standalone.js'), runtime]);
for (const component of ['musikcoreSDK', 'musikcorePlugins']) {
  run('cmake', ['--install', build, '--prefix', sdk, '--component', component]);
}
console.log(`SDK staged at ${sdk}\nBuild/audit log: ${log}`);
