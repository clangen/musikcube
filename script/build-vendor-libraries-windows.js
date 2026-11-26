// Load the x64 MSVC environment, build packages with vcpkg, and build FFmpeg in portable MSYS2.
// Keep tools, downloads, source trees, and staging directories under vendor/.build/.
// Audit staged binaries, then publish headers to vendor/include/ and libraries to vendor/win64/{lib,dll}/.
// --clean-work removes vendor/.build/ after publishing.

const { spawnSync } = require('node:child_process');
const { createHash } = require('node:crypto');
const {
  copyFileSync, cpSync, existsSync, mkdirSync, mkdtempSync,
  readFileSync, readdirSync, realpathSync, renameSync, rmSync, unlinkSync, writeFileSync,
} = require('node:fs');
const { delimiter, dirname, extname, join, resolve } = require('node:path');
const { parseArgs } = require('node:util');
const sanitizePresets = require('./vendor-packages/unix/sanitize-projectm-presets');

const script = __dirname;
const support = join(script, 'vendor-packages', 'windows');
const vendor = join(script, '..', 'vendor');
const work = join(vendor, '.build');
const triplet = 'musikcube-x64';
const vcpkgTag = '2025.10.17';
const ffmpegTag = 'n8.0';
const msysVersion = '2025-12-13';
const projectmPresetsRevision = 'c9a433306d8d4d24c0a0b091324ec2e4492d1828';

const sharedPackages = {
  curl: 'curl[core,ssl]', openssl: 'openssl', lame: 'mp3lame', libopenmpt: 'libopenmpt',
};
const staticPackages = ['libgme', 'taglib', 'zlib', 'libmicrohttpd', 'projectm', 'sdl2'];
const sharedLibraries = {
  curl: [['libcurl', 'libcurl', 'libcurl_imp', 'curl']],
  openssl: [['openssl', 'libssl', 'ssl'], ['openssl', 'libcrypto', 'crypto']],
  lame: [['lame', 'libmp3lame', 'mp3lame', 'lame', 'lame_enc']],
  libopenmpt: [['libopenmpt', 'libopenmpt', 'openmpt']],
};
const staticLibraries = {
  'gme.lib': ['lib/gme.lib'],
  'gmeD.lib': ['debug/lib/gme.lib', 'debug/lib/gmed.lib'],
  'tag.lib': ['lib/tag.lib'],
  'tagD.lib': ['debug/lib/tag.lib', 'debug/lib/tagd.lib'],
  'zlibstatic.lib': ['lib/zlib.lib', 'lib/zlibstatic.lib'],
  'zlibstaticd.lib': ['debug/lib/zlibd.lib', 'debug/lib/zlib.lib', 'debug/lib/zlibstaticd.lib'],
  'libmicrohttpd.lib': ['lib/libmicrohttpd.lib', 'lib/microhttpd.lib'],
  'libmicrohttpdD.lib': [
    'debug/lib/libmicrohttpd_d.lib', 'debug/lib/libmicrohttpd.lib',
    'debug/lib/libmicrohttpdd.lib', 'debug/lib/microhttpd.lib',
  ],
};

function run(program, args, { capture = false, ...options } = {}) {
  const result = spawnSync(program, args, {
    ...options,
    encoding: 'utf8',
    stdio: capture ? ['ignore', 'pipe', 'inherit'] : 'inherit',
    maxBuffer: 32 * 1024 * 1024,
  });

  if (result.error) throw result.error;
  if (result.status !== 0) {
    throw new Error(program + ' exited with ' + (result.signal ?? result.status));
  }

  return result.stdout;
}

function runBatch(command, options) {
  // Use a fixed command line. The path to the batch file is supplied as cwd.
  return run(process.env.ComSpec ?? 'cmd.exe', ['/d', '/c', command], {
    ...options, windowsVerbatimArguments: true,
  });
}

function findVisualStudio() {
  const vswhere = join(process.env['ProgramFiles(x86)'], 'Microsoft Visual Studio', 'Installer', 'vswhere.exe');

  const root = run(vswhere, [
    '-latest', '-products', '*', '-requires',
    'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath',
  ], { capture: true }).trim();

  if (!root) throw new Error('Visual Studio x64 C++ Build Tools are required.');

  const auxiliary = join(root, 'VC', 'Auxiliary', 'Build');
  const version = readFileSync(join(auxiliary, 'Microsoft.VCToolsVersion.default.txt'), 'utf8').trim();
  const native = join(root, 'VC', 'Tools', 'MSVC', version, 'bin', 'Hostx64', 'x64');
  const cmake = join(root, 'Common7', 'IDE', 'CommonExtensions', 'Microsoft', 'CMake', 'CMake', 'bin');

  return { auxiliary, native, cmake, dumpbin: join(native, 'dumpbin.exe') };
}

function loadMsvcEnvironment(tools) {
  const output = runBatch('call vcvars64.bat >nul && set', { cwd: tools.auxiliary, capture: true });

  const entries = output.split(/\r?\n/).flatMap(line => {
    const separator = line.indexOf('=');
    return separator > 0 ? [[line.slice(0, separator).toUpperCase(), line.slice(separator + 1)]] : [];
  });

  const env = Object.fromEntries(entries);
  if (existsSync(join(tools.cmake, 'cmake.exe'))) env.PATH = tools.cmake + delimiter + env.PATH;
  env.VCPKG_DISABLE_METRICS = '1';
  env.VCPKG_DEFAULT_BINARY_CACHE = join(work, 'binary-cache');
  mkdirSync(env.VCPKG_DEFAULT_BINARY_CACHE, { recursive: true });

  return env;
}

function readDigest(file) {
  const digest = readFileSync(file, 'utf8').trim().split(/\s+/)[0].toLowerCase();
  if (!/^[a-f0-9]{64}$/.test(digest)) throw new Error('Invalid SHA-256 checksum: ' + file);
  return digest;
}

function verifyArchive(file, expected) {
  if (createHash('sha256').update(readFileSync(file)).digest('hex') !== expected) {
    throw new Error('MSYS2 archive failed the SHA-256 check: ' + file);
  }
}

function download(url, path) {
  run(join(process.env.SystemRoot, 'System32', 'curl.exe'), [
    '--fail', '--location', '--output', path, url,
  ]);
}

function missingMsysTools(root) {
  return ['bash.exe', 'make.exe', 'pkg-config.exe', 'diff.exe']
    .filter(tool => !existsSync(join(root, 'usr', 'bin', tool)));
}

function provisionMsys2(root, portable) {
  const missing = missingMsysTools(root);

  if (!missing.length) return;
  if (!portable) throw new Error('MSYS2 tools missing under ' + root + ': ' + missing.join(', '));

  const bash = join(root, 'usr', 'bin', 'bash.exe');
  if (!existsSync(bash)) {
    const name = 'msys2-base-x86_64-' + msysVersion.replaceAll('-', '') + '.tar.xz';
    const release = 'https://github.com/msys2/msys2-installer/releases/download/' + msysVersion;
    const downloads = join(work, 'downloads');
    const archive = join(downloads, name);
    const checksum = archive + '.sha256';

    mkdirSync(downloads, { recursive: true });

    if (!existsSync(checksum)) {
      download(release + '/' + name + '.sha256', checksum + '.part');
      readDigest(checksum + '.part');
      renameSync(checksum + '.part', checksum);
    }

    const digest = readDigest(checksum);

    if (!existsSync(archive)) {
      download(release + '/' + name, archive + '.part');
      verifyArchive(archive + '.part', digest);
      renameSync(archive + '.part', archive);
    } else {
      verifyArchive(archive, digest);
    }

    const extraction = mkdtempSync(join(work, 'msys2-extract-'));
    run(join(process.env.SystemRoot, 'System32', 'tar.exe'), ['-xf', archive, '-C', extraction]);

    if (!existsSync(join(extraction, 'msys64', 'usr', 'bin', 'bash.exe'))) {
      throw new Error('Unexpected layout in ' + archive);
    }
    renameSync(extraction, dirname(root));
  }

  run(bash, ['--login', '-c', [
    "trap 'gpgconf --homedir /etc/pacman.d/gnupg --kill all' EXIT",
    'pacman -Sy --noconfirm --needed msys2-keyring && pacman -S --noconfirm --needed make pkgconf diffutils',
  ].join('; ')], {
    env: { ...process.env, MSYSTEM: 'MSYS', CHERE_INVOKING: '1', MSYS2_PATH_TYPE: 'minimal' },
  });

  const stillMissing = missingMsysTools(root);
  if (stillMissing.length) throw new Error('Portable MSYS2 is missing ' + stillMissing.join(', '));
}

function cloneTag(url, tag, destination) {
  if (!existsSync(join(destination, '.git'))) {
    run('git.exe', ['clone', '--depth', '1', '--branch', tag, url, destination]);
  }
}

function buildPackages(packages, env) {
  const root = join(work, 'vcpkg-' + vcpkgTag);
  cloneTag('https://github.com/microsoft/vcpkg.git', vcpkgTag, root);

  const vcpkg = join(root, 'vcpkg.exe');
  if (!existsSync(vcpkg)) {
    runBatch('call bootstrap-vcpkg.bat -disableMetrics', { cwd: root, env });
  }

  run(vcpkg, [
    'install', '--classic', '--triplet=' + triplet, '--host-triplet=x64-windows',
    '--vcpkg-root=' + root, '--overlay-triplets=' + join(support, 'triplets'), ...packages,
  ], { cwd: root, env });

  return join(root, 'installed', triplet);
}

function buildFfmpeg(msys, installed, tools, env) {
  const source = join(work, 'ffmpeg-' + ffmpegTag);
  cloneTag('https://github.com/FFmpeg/FFmpeg.git', ffmpegTag, source);

  const build = mkdtempSync(join(work, 'ffmpeg-stage-'));
  const install = join(build, 'install');
  mkdirSync(install);

  const bin = join(msys, 'usr', 'bin');
  run(join(bin, 'bash.exe'), [
    join(support, 'build-ffmpeg.sh'), source, join(support, 'ffmpeg-options.txt'),
    install, installed, build, tools.native,
  ], { env: { ...env, PATH: bin + delimiter + env.PATH } });

  return install;
}

function mkdir(path) {
  mkdirSync(path, { recursive: true });
  return path;
}

function stageShared(stage, dumpbin, prefix, [group, name, ...aliases]) {
  const candidates = [name, ...aliases].flatMap(candidate =>
    ['lib', 'bin'].map(directory => join(prefix, directory, candidate + '.lib')));
  const library = candidates.find(existsSync);
  if (!library) throw new Error('Missing import library for ' + name + ' under ' + prefix);

  const output = run(dumpbin, ['/headers', library], { capture: true });
  const dlls = [...new Set([...output.matchAll(/DLL name\s*:\s*(\S+\.dll)/gi)].map(match => match[1]))];
  if (dlls.length !== 1) throw new Error('Expected one DLL in ' + library);

  copyFileSync(join(prefix, 'bin', dlls[0]), join(mkdir(join(stage.dll, group)), dlls[0]));
  copyFileSync(library, join(stage.lib, name + '.lib'));
}

function stageBundle(installed, ffmpeg, library, dumpbin) {
  const dir = mkdtempSync(join(work, 'stage-'));
  const stage = {
    headers: mkdir(join(dir, 'include')),
    lib: mkdir(join(dir, 'win64', 'lib')),
    dll: mkdir(join(dir, 'win64', 'dll')),
  };

  const includes = name => library === 'all' ? name !== 'curl' && name !== 'openssl' : library === name;
  if (installed) cpSync(join(installed, 'include'), stage.headers, { recursive: true });

  for (const [packageName, libraries] of Object.entries(sharedLibraries)) {
    if (!includes(packageName)) continue;
    for (const spec of libraries) stageShared(stage, dumpbin, installed, spec);
  }

  if (includes('lame')) copyFileSync(join(stage.lib, 'libmp3lame.lib'), join(stage.lib, 'libmp3lameD.lib'));

  const openmptConfig = join(stage.headers, 'libopenmpt', 'libopenmpt_config.h');
  if (existsSync(openmptConfig)) {
    // The decoder uses the C API. The header still disables C++ API imports.
    const config = readFileSync(openmptConfig, 'utf8').replace(
      /^#error "C\+\+ interface is disabled if libopenmpt is built as a DLL and the runtime is statically linked\.[^\r\n]*"/m,
      '/* Allow the C API in C++ translation units. */',
    );
    writeFileSync(openmptConfig, config);
  }

  if (ffmpeg) {
    for (const name of ['avcodec', 'avformat', 'avutil', 'swresample']) {
      stageShared(stage, dumpbin, ffmpeg, ['ffmpeg', name + '-musikcube', name]);
    }
    cpSync(join(ffmpeg, 'include'), stage.headers, { recursive: true });
  }

  if (includes('static')) {
    for (const [name, candidates] of Object.entries(staticLibraries)) {
      const source = candidates.map(path => join(installed, path)).find(existsSync);
      if (!source) throw new Error('Missing static archive for ' + name + ' under ' + installed);
      copyFileSync(source, join(stage.lib, name));
    }
    for (const config of ['lib', 'debug/lib']) {
      const suffix = config === 'lib' ? '' : 'D';
      for (const [name, releaseName, debugName] of [
        ['projectM-4', 'libprojectM-4', 'libprojectM-4d'],
        ['projectM-eval', 'projectM_eval', 'projectM_eval'],
        ['projectM-eel', 'projectM_ns-eel2', 'projectM_ns-eel2'],
        ['glew32', 'glew32', 'glew32d'], ['SDL2', 'SDL2-static', 'SDL2-staticd'],
      ]) {
        const source = join(installed, config, (suffix ? debugName : releaseName) + '.lib');
        if (!existsSync(source)) throw new Error('Missing static archive: ' + source);
        copyFileSync(source, join(stage.lib, name + suffix + '.lib'));
      }
    }
    const imgui = join(work, 'imgui-1.91.9b');
    const presets = join(work, 'milkdrop2-musikcube-' + projectmPresetsRevision);
    cloneTag('https://github.com/ocornut/imgui.git', 'v1.91.9b', imgui);
    if (!existsSync(join(presets, '.git'))) {
      run('git.exe', ['init', presets]);
      run('git.exe', ['-C', presets, 'fetch', '--depth', '1', 'https://github.com/clangen/milkdrop2-musikcube.git',
        projectmPresetsRevision]);
      run('git.exe', ['-C', presets, 'checkout', '--detach', 'FETCH_HEAD']);
    }
    cpSync(imgui, join(stage.headers, 'projectm-imgui'), { recursive: true, filter: path => !path.includes(join(imgui, '.git')) });
    stage.presets = join(dir, 'projectM');
    cpSync(join(presets, 'resources', 'Milkdrop2', 'presets'), stage.presets, { recursive: true });
    sanitizePresets(stage.presets);
    writeFileSync(join(stage.presets, 'README.md'),
      `Milkdrop2 presets from https://github.com/clangen/milkdrop2-musikcube, revision ${projectmPresetsRevision}.\n`);
  }

  return stage;
}

function binariesIn(directory) {
  return readdirSync(directory, { withFileTypes: true }).flatMap(entry => {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) return binariesIn(path);
    return entry.isFile() && ['.dll', '.exe'].includes(extname(entry.name).toLowerCase()) ? [path] : [];
  });
}

function auditImports(dumpbin, directory) {
  const binaries = binariesIn(directory);
  if (!binaries.length) throw new Error('No Windows binaries in ' + directory);

  const failures = [];
  const forbidden = /^\s*((?:msys-|cyg|libgcc|libstdc\+\+|libwinpthread|libssp|libunwind|libc\+\+|vcruntime|msvcp|msvcr\d|concrt|ucrtbase|api-ms-win-crt)[\w+.-]*\.dll)\s*$/gim;
  for (const binary of binaries) {
    if (!/8664 machine/.test(run(dumpbin, ['/headers', binary], { capture: true }))) {
      failures.push(binary + ': expected x64');
    }
    const imports = run(dumpbin, ['/dependents', binary], { capture: true });
    for (const match of imports.matchAll(forbidden)) failures.push(binary + ': ' + match[1]);
  }

  if (failures.length) throw new Error('Bundle failed its architecture or runtime check:\n' + failures.join('\n'));
  console.log('Checked ' + binaries.length + ' x64 binaries in ' + directory + '.');
}

function publishBundle(stage) {
  cpSync(stage.headers, join(vendor, 'include'), { recursive: true });
  cpSync(stage.lib, join(vendor, 'win64', 'lib'), { recursive: true });
  if (stage.presets) {
    const target = join(vendor, 'share', 'projectM');
    rmSync(target, { recursive: true, force: true });
    cpSync(stage.presets, target, { recursive: true });
  }

  for (const group of readdirSync(stage.dll)) {
    const source = join(stage.dll, group);
    const target = join(vendor, 'win64', 'dll', group);
    cpSync(source, target, { recursive: true });

    const expected = new Set(readdirSync(source).map(name => name.toLowerCase()));
    for (const entry of readdirSync(target, { withFileTypes: true })) {
      if (entry.isFile() && /\.dll$/i.test(entry.name) && !expected.has(entry.name.toLowerCase())) {
        unlinkSync(join(target, entry.name));
      }
    }
  }

  console.log('Windows vendor libraries staged under ' + vendor + '.');
}

function cleanWorkDirectory() {
  const expected = join(realpathSync(join(script, '..')), 'vendor', '.build');
  const target = realpathSync(work);
  if (target !== expected) {
    throw new Error('Bundle published. Refusing to remove a redirected work directory: ' + target);
  }

  try {
    rmSync(target, { recursive: true, maxRetries: 3, retryDelay: 200 });
  } catch (error) {
    throw new Error('Bundle published, but cleanup failed: ' + error.message);
  }

  console.log('Removed build tools, downloads, and caches from ' + target + '.');
}

function main() {
  const { values } = parseArgs({
    options: {
      library: { type: 'string', default: 'all' },
      'msys-root': { type: 'string' },
      'ffmpeg-install': { type: 'string' },
      preflight: { type: 'boolean' },
      'audit-directory': { type: 'string' },
      'clean-work': { type: 'boolean' },
      help: { type: 'boolean', short: 'h' },
    },
  });

  if (values.help) {
    console.log([
      'Usage: node script/build-vendor-libraries.js [options]',
      '  --library <all|curl|openssl|lame|libopenmpt|ffmpeg|static>  (curl and openssl are optional)',
      '  --msys-root <path>       Use an existing MSYS2 installation',
      '  --ffmpeg-install <path>  Stage a completed FFmpeg installation',
      '  --preflight              Check tools without downloading or building',
      '  --audit-directory <path> Check an existing set of Windows binaries',
      '  --clean-work             Remove vendor/.build/ after publishing',
    ].join('\n'));
    return;
  }

  if (process.platform !== 'win32') throw new Error('This entry point builds Windows vendor libraries.');

  const library = values.library.toLowerCase();
  if (!['all', 'static', 'ffmpeg', ...Object.keys(sharedPackages)].includes(library)) {
    throw new Error('Unknown library: ' + values.library);
  }

  /* Audit and preflight can exit before provisioning build tools. */
  const tools = findVisualStudio();
  if (values['audit-directory']) {
    auditImports(tools.dumpbin, resolve(values['audit-directory']));
    return;
  }

  const needsFfmpeg = library === 'all' || library === 'ffmpeg';
  const reuse = needsFfmpeg && values['ffmpeg-install'] ? resolve(values['ffmpeg-install']) : null;
  const portable = !values['msys-root'];
  const msys = portable ? join(work, 'msys2-' + msysVersion, 'msys64') : resolve(values['msys-root']);

  if (values.preflight) {
    console.log('Visual Studio x64 tools: ' + tools.native);
    console.log('Git: ' + run('git.exe', ['--version'], { capture: true }).trim());
    if (needsFfmpeg && !reuse) {
      const missing = missingMsysTools(msys);
      if (missing.length && !portable) throw new Error('Missing MSYS2 tools: ' + missing.join(', '));
      console.log((missing.length ? 'MSYS2 will be provisioned under ' : 'MSYS2 build tools: ') + msys);
    }
    return;
  }

  const env = loadMsvcEnvironment(tools);
  if (needsFfmpeg && !reuse) provisionMsys2(msys, portable);

  /* Build the selected packages and any codecs FFmpeg needs. */
  const packages = library === 'all' ? [sharedPackages.lame, sharedPackages.libopenmpt, ...staticPackages]
    : library === 'static' ? [...staticPackages] : sharedPackages[library] ? [sharedPackages[library]] : [];
  if (needsFfmpeg && !reuse) packages.push('opus', 'libvorbis');

  const installed = packages.length ? buildPackages(packages, env) : null;
  const ffmpeg = needsFfmpeg ? reuse ?? buildFfmpeg(msys, installed, tools, env) : null;

  /* Audit the staged DLLs before publishing the vendor bundle. */
  const stage = stageBundle(installed, ffmpeg, library, tools.dumpbin);
  if (library !== 'static') auditImports(tools.dumpbin, stage.dll);
  publishBundle(stage);

  if (values['clean-work']) cleanWorkDirectory();
}

try {
  main();
} catch (error) {
  console.error(error.message);
  process.exitCode = 1;
}
