/**
 * We want to statically link as many dependencies as possible to "standalone"
 * executables and plugins to make it as easy as possible to just extract and
 * run the app and daemon without needing to install any additional packages.
 *
 * This script scans binaries produced by the build process to ensure there
 * are no unexpected libraries being dynamically linked. foreign ELF files
 * are inspected with readelf without executing the target loader.
 *
 * If dependencies undergo major version bumps, we often need to update the
 * set of expected libraries that are used to perform the scan.
 */

const { promisify } = require('util');
const execFile = promisify(require('child_process').execFile);
const { basename, dirname, isAbsolute, join, relative, resolve, sep } = require('path');
const { readdir, realpath, stat } = require('fs').promises;
const env = { ...process.env, LC_ALL: 'C' };
/* Inspect the package's own lookup paths. */
delete env.LD_LIBRARY_PATH;
delete env.LD_PRELOAD;
delete env.LD_AUDIT;

const mac = process.platform === 'darwin';

const filetype = mac ? '.dylib' : '.so';

/* Plugins whose dependencies are supplied by the host. */
const ignoredLibraries = mac
  ? new Set([])
  : new Set([
    'libalsaout.so',
    'libmpris.so',
    'libpipewireout.so',
    'libpulseout.so',
    'libsndioout.so',
  ]);

/* Direct host dependencies permitted for optional Linux plugins. */
const optionalPluginLibraries = new Map([
  ['libalsaout.so', 'libasound.so.2'],
  ['libmpris.so', 'libsystemd.so.0'],
  ['libpipewireout.so', 'libpipewire-0.3.so.0'],
  ['libportaudioout.so', 'libportaudio.so.2'],
  ['libpulseout.so', 'libpulse.so.0'],
  ['libsndioout.so', 'libsndio.so.7.0'],
]);

/* ELF interpreter names used by supported Linux architectures. */
const linuxLoaders = [
  'ld-linux-x86-64.so.2',
  'ld-linux.so.2',
  'ld-linux-armhf.so.3',
  'ld-linux-aarch64.so.1',
];

/* Linux runtime libraries allowed in both scans. */
const linuxRuntimeLibraries = [
  ...linuxLoaders, 'libc.so.6', 'libdl.so.2', 'libgcc_s.so.1', 'libm.so.6',
  'libpthread.so.0', 'librt.so.1', 'libstdc++.so.6', 'libz.so.1',
];

/* macOS runtime libraries and frameworks allowed in both scans. */
const macRuntimeLibraries = [
  '/usr/lib/libc++.1.dylib', '/usr/lib/libSystem.B.dylib', '/usr/lib/libz.1.dylib',
  '/usr/lib/libiconv.2.dylib', '/usr/lib/libresolv.9.dylib', '/usr/lib/libobjc.A.dylib',
  '/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation',
  '/System/Library/Frameworks/CoreMedia.framework/Versions/A/CoreMedia',
  '/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices',
  '/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo',
  '/System/Library/Frameworks/Security.framework/Versions/A/Security',
  '/System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration',
];

/* External dependencies allowed in the finished archive. */
const validLibraries = mac
  ? new Set([
    ...macRuntimeLibraries,
    '/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit',
    '/System/Library/Frameworks/ApplicationServices.framework/Versions/A/ApplicationServices',
    '/System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox',
    '/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon',
    '/System/Library/Frameworks/CoreAudio.framework/Versions/A/CoreAudio',
    '/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics',
    '/System/Library/Frameworks/CoreMedia.framework/Versions/A/CoreMedia',
    '/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices',
    '/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo',
    '/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation',
    '/opt/homebrew/opt/portaudio/lib/libportaudio.2.dylib',
    '/usr/local/opt/portaudio/lib/libportaudio.2.dylib',
  ])
  : new Set([
    ...linuxRuntimeLibraries,
    '/lib/ld-linux-armhf.so.3',
    '/lib64/ld-linux-x86-64.so.2',
    '/usr/lib/arm-linux-gnueabihf/libarmmem-${PLATFORM}.so',
    '/usr/lib/arm-linux-gnueabi/libarmmem-${PLATFORM}.so',
    'linux-vdso.so.1',
    'libportaudio.so.2',
    'libasound.so.2',
    'libjack.so.0',
    'libdb-5.3.so',
  ]);

let errors = 0;

const vendorMode = process.argv[2] === '--vendor';
const [packagePath, ...args] = process.argv.slice(vendorMode ? 3 : 2);
const elfTargets = {
  armv6: { machine: 'ARM', elfClass: 'ELF32', cpu: 'v6', loader: '/lib/ld-linux-armhf.so.3' },
  armv8: { machine: 'ARM', elfClass: 'ELF32', cpu: 'v8', loader: '/lib/ld-linux-armhf.so.3' },
  x86_64: { machine: 'Advanced Micro Devices X86-64', elfClass: 'ELF64', loader: '/lib64/ld-linux-x86-64.so.2' },
};
/* ARM1176 supports the ARMv6KZ profile and Thumb-1. */
const armv6Architectures = new Set(['v4', 'v4T', 'v5T', 'v5TE', 'v5TEJ', 'v6', 'v6K', 'v6KZ']);

let glibcMax;
let target;
let invalid = !packagePath;
for (let i = 0; i < args.length; i += 2) {
  const [flag, value] = args.slice(i, i + 2);
  if (flag === '--glibc-max' && !glibcMax && /^\d+\.\d+(\.\d+)?$/.test(value)) {
    glibcMax = value;
  } else if (flag === '--target' && !target && Object.prototype.hasOwnProperty.call(elfTargets, value)) {
    target = elfTargets[value];
  } else {
    invalid = true;
  }
}
if (invalid || (mac && (target || glibcMax))) {
  console.log('usage: node scan-standalone.js [--vendor] <path> [--glibc-max <version>] [--target armv6|armv8|x86_64]');
  process.exit(1);
}

const root = resolve(packagePath);

const ls = async (leaf) => {
  const output = await readdir(`${root}/${leaf}`);
  const files = output
    .filter((fn) => fn.indexOf(filetype) !== -1)
    .filter((fn) => target || glibcMax || !ignoredLibraries.has(fn));
  return files;
};

const within = (root, path) => {
  const child = relative(root, path);
  return child && child !== '..' && !child.startsWith('..' + sep) && !isAbsolute(child);
};

const neededLinux = async (file) => {
  const { stdout } = await execFile('readelf', ['-d', file], { env });
  return [...stdout.matchAll(/\(NEEDED\).*?\[([^\]]+)\]/g)].map(([, name]) => name);
};

/* packaged zlib and the static C++ runtime must not resolve from the host. */
const targetRuntime = new Set(['libc.so.6', 'libm.so.6', 'libdl.so.2', 'libpthread.so.0',
  'librt.so.1', 'libgcc_s.so.1']);

const targetAllows = (name) => targetRuntime.has(name) || name === basename(target.loader) ||
  (target.cpu === 'v6' && name === 'libatomic.so.1');

const elfProblems = async (file, executable = false) => {
  const { stdout } = await execFile('readelf', ['-h', '-A', '-l', file], { env });
  const problems = [];
  const field = (name) => stdout.match(new RegExp('^\\s*' + name + ':\\s*(.+)$', 'm'))?.[1].trim();
  if (field('Class') !== target.elfClass || field('Machine') !== target.machine ||
      !field('Data')?.includes('little endian')) {
    problems.push(`wrong ELF architecture (expected ${target.elfClass} ${target.machine})`);
  }
  const type = field('Type')?.split(/\s+/)[0];
  if (type !== 'DYN' && (!executable || type !== 'EXEC')) {
    problems.push(`unexpected ELF type: ${type ?? '(missing)'}`);
  }
  if (target.cpu) {
    const cpu = field('Tag_CPU_arch');
    const flags = field('Flags') ?? '';
    const compatible = armv6Architectures.has(cpu) ||
      (target.cpu === 'v8' && ['v6T2', 'v7', 'v8'].includes(cpu));
    const float = field('Tag_FP_arch');
    const armv6Features = target.cpu !== 'v6' ||
      ((!float || float === 'VFPv2' || float === 'VFPv1') && field('Tag_THUMB_ISA_use') !== 'Thumb-2');
    if (!compatible || !armv6Features || (executable && cpu !== target.cpu) ||
        !flags.includes('Version5 EABI') || !flags.includes('hard-float ABI')) {
      problems.push(`incompatible ARM attributes (expected EABI5 hard-float ARM${target.cpu}${executable ? '' : ' or older'})`);
    }
  }
  const loader = stdout.match(/Requesting program interpreter: ([^\]]+)/)?.[1];
  if ((executable || loader) && loader !== target.loader) {
    problems.push(`unexpected ELF interpreter: ${loader ?? '(missing)'}`);
  }
  return problems;
};

const packagedElfProblems = async (file, packageRoot) => {
  const { stdout } = await execFile('readelf', ['-d', file], { env });
  const needed = [...stdout.matchAll(/\(NEEDED\).*?\[([^\]]+)\]/g)].map(([, name]) => name);
  const paths = [...stdout.matchAll(/\((RUNPATH|RPATH)\).*?\[([^\]]*)\]/g)];
  const effective = paths.find(([, tag]) => tag === 'RUNPATH') ?? paths[0];
  const problems = [];
  const search = [];
  for (const path of effective ? effective[2].split(':') : []) {
    const origin = path.match(/^\$(?:ORIGIN|\{ORIGIN\})(?:\/(.*))?$/);
    if (!origin) {
      problems.push(`non-relocatable search path: ${path}`);
      continue;
    }
    const directory = resolve(dirname(file), origin[1] ?? '');
    if (directory !== packageRoot && !within(packageRoot, directory)) {
      problems.push(`search path leaves package: ${path}`);
      continue;
    }
    search.push(directory);
  }
  const hostLibrary = dirname(file) === join(packageRoot, 'plugins')
    ? optionalPluginLibraries.get(basename(file)) : undefined;
  const scannedDirectories = [join(packageRoot, 'lib'), join(packageRoot, 'plugins')];
  for (const name of needed) {
    if (targetAllows(name) || name === hostLibrary) continue;
    let found = false;
    if (name === basename(name)) {
      for (const directory of search) {
        try {
          const candidate = await realpath(join(directory, name));
          const scanned = candidate === join(packageRoot, 'libmusikcore.so') ||
            (scannedDirectories.includes(dirname(candidate)) && basename(candidate).includes(filetype));
          if (scanned && (await stat(candidate)).isFile()) {
            found = true;
            break;
          }
        } catch (error) {
          if (error.code !== 'ENOENT' && error.code !== 'ENOTDIR') throw error;
        }
      }
    }
    if (!found) problems.push(`${name} (not found on packaged search paths)`);
  }
  return problems;
};

const glibcProblems = async (file) => {
  if (!glibcMax) return [];

  const { stdout } = await execFile('readelf', ['--version-info', file], { env });
  const needs = stdout.split('Version needs section')[1] || '';
  const limit = glibcMax.split('.').map(Number);
  const problems = new Set();
  for (const [, version] of needs.matchAll(/Name: GLIBC_(\S+)/g)) {
    if (/^\d+\.\d+(\.\d+)?$/.test(version)) {
      const parts = version.split('.').map(Number);
      const difference = parts.findIndex((part, index) => part !== (limit[index] || 0));
      if (difference < 0 || parts[difference] < (limit[difference] || 0)) continue;
    }
    problems.add(`GLIBC_${version} (target glibc ${glibcMax})`);
  }
  return [...problems];
};

const loadCommandsMac = async (file) => {
  const { stdout } = await execFile('otool', ['-l', file], { env });
  const needed = [...stdout.matchAll(
    /^\s*cmd LC_(?:LOAD|LOAD_WEAK|REEXPORT|LAZY_LOAD|LOAD_UPWARD)_DYLIB\s*\n\s*cmdsize \d+\s*\n\s*name (.+?) \(offset \d+\)/gm
  )].map(([, name]) => name);
  const rpaths = [...stdout.matchAll(
    /^\s*cmd LC_RPATH\s*\n\s*cmdsize \d+\s*\n\s*path (.+?) \(offset \d+\)/gm
  )].map(([, path]) => path);
  /* Universal binaries can list the same dependency in multiple slices. */
  return { needed: [...new Set(needed)], rpaths: [...new Set(rpaths)] };
};

const neededMac = async (file) => (await loadCommandsMac(file)).needed;

const lddLinux = async (fn) => {
  const file = join(root, fn);
  const packageRoot = await realpath(root);
  const needed = await neededLinux(file);
  const { stdout } = await execFile('ldd', [file], { env });
  const resolved = new Map(); /* Dependency name to path reported by ldd. */
  const problems = new Set(); /* Unique diagnostics for this binary. */

  for (const line of stdout.split('\n')) {
    const dependency = line.trim().match(/^(\S+) => (.+?)(?: \(0x[\da-f]+\))?$/i);
    if (!dependency) continue;
    const [, name, location] = dependency;
    if (location === 'not found') problems.add(`${name} (not found)`);
    else resolved.set(name, location);
  }

  /* Each shipped library is checked separately. The target system supplies */
  /* dependencies of system libraries such as PortAudio and its audio backends. */
  for (const name of needed) {
    if (validLibraries.has(name) || problems.has(`${name} (not found)`)) continue;
    const location = resolved.get(name);
    if (name === basename(name) && location && isAbsolute(location)) {
      if (within(packageRoot, await realpath(location))) continue;
    }
    problems.add(location ? `${name} (resolved to ${location})` : name);
  }
  return [...problems];
};

let executableRpathRoots;

const expandMacPath = (path, file, packageRoot) => {
  const substitutions = [
    ['@loader_path', dirname(file)],
    ['@executable_path', packageRoot],
  ];
  for (const [prefix, directory] of substitutions) {
    if (path === prefix) return directory;
    if (path.startsWith(prefix + '/')) return resolve(directory, path.slice(prefix.length + 1));
  }
  return isAbsolute(path) ? resolve(path) : undefined;
};

const packagedMacFile = async (packageRoot, candidate) => {
  try {
    const file = await realpath(candidate);
    return within(packageRoot, file) && (await stat(file)).isFile();
  } catch (error) {
    if (error.code === 'ENOENT') return false;
    throw error;
  }
};

const macExecutableRpathRoots = async (packageRoot) => {
  if (!executableRpathRoots) {
    executableRpathRoots = Promise.all(['musikcube', 'musikcubed'].map(async (name) => {
      const file = await realpath(join(root, name));
      const { rpaths } = await loadCommandsMac(file);
      return rpaths.map((path) => expandMacPath(path, file, packageRoot));
    })).then((paths) => [...new Set(paths.flat().filter((path) =>
      path && (path === packageRoot || within(packageRoot, path))))]);
  }
  return executableRpathRoots;
};

const lddMac = async (fn) => {
  const packageRoot = await realpath(root);
  const file = await realpath(join(root, fn));
  const { needed, rpaths } = await loadCommandsMac(file);
  const searchRoots = new Set(rpaths.map((path) => expandMacPath(path, file, packageRoot)));
  if (fn !== 'musikcube' && fn !== 'musikcubed') {
    for (const path of await macExecutableRpathRoots(packageRoot)) searchRoots.add(path);
  }
  const problems = [];

  for (const name of needed) {
    if (validLibraries.has(name)) continue;

    if (name.startsWith('@rpath/')) {
      const suffix = name.slice('@rpath/'.length);
      let found = false;
      for (const directory of searchRoots) {
        if (directory && await packagedMacFile(packageRoot, resolve(directory, suffix))) {
          found = true;
          break;
        }
      }
      if (!found) problems.push(`${name} (not found in package rpaths)`);
      continue;
    }

    if (name.startsWith('@loader_path/') || name.startsWith('@executable_path/')) {
      const candidate = expandMacPath(name, file, packageRoot);
      if (!candidate || !await packagedMacFile(packageRoot, candidate)) {
        problems.push(`${name} (not found in package)`);
      }
      continue;
    }

    problems.push(name);
  }
  return problems;
};

/* External dependencies allowed before archive staging. */
const vendorSystemLibraries = new Set(mac ? macRuntimeLibraries : linuxRuntimeLibraries);

const vendorLibraries = async (directory) => {
  const files = [];
  for (const entry of await readdir(directory, { withFileTypes: true })) {
    const path = join(directory, entry.name);
    if (entry.isDirectory()) files.push(...await vendorLibraries(path));
    else if (/\.dylib$|\.so(?:\.[0-9]+)*$/.test(entry.name)) files.push(path);
  }
  return files;
};

const scanVendor = async () => {
  const directory = await realpath(root);
  const candidates = await vendorLibraries(directory);
  const aliases = new Set(); /* Installed paths, including versioned symlinks. */
  const files = new Set(); /* Resolved files, each scanned once. */
  for (const candidate of candidates) {
    const file = await realpath(candidate);
    if (!within(directory, file) || !(await stat(file)).isFile()) {
      throw new Error(`Invalid vendor library path: ${candidate}`);
    }
    aliases.add(candidate);
    files.add(file);
  }
  if (!files.size) throw new Error(`No shared libraries found in ${directory}`);

  for (const file of files) {
    const needed = await (mac ? neededMac(file) : neededLinux(file));
    const problems = needed.filter((name) => {
      if (target ? targetAllows(name) : vendorSystemLibraries.has(name)) return false;
      if (!mac) return basename(name) !== name || !aliases.has(join(directory, name));
      if (name.startsWith('@rpath/')) return !aliases.has(resolve(directory, name.slice(7)));
      if (name.startsWith('@loader_path/')) return !aliases.has(resolve(dirname(file), name.slice(13)));
      return true;
    });
    report(relative(directory, file), [...problems, ...await glibcProblems(file),
      ...(target ? await elfProblems(file) : [])]);
  }
};

const ldd = async (fn) => {
  if (target) {
    const directory = await realpath(root);
    const file = await realpath(join(directory, fn));
    if (!within(directory, file)) throw new Error(`Binary is outside the package: ${fn}`);
    report(fn, [...await elfProblems(file, fn === 'musikcube' || fn === 'musikcubed'),
      ...await packagedElfProblems(file, directory), ...await glibcProblems(file)]);
    return;
  }
  /* Optional plugins use the host's stack, but still need the target glibc ABI. */
  const optionalLibrary = glibcMax && optionalPluginLibraries.get(basename(fn));
  const problems = optionalLibrary
    ? (await neededLinux(join(root, fn))).filter((name) =>
      name !== optionalLibrary && !linuxRuntimeLibraries.includes(name))
    : ignoredLibraries.has(basename(fn)) ? [] : mac ? await lddMac(fn) : await lddLinux(fn);
  report(fn, [...problems, ...await glibcProblems(join(root, fn))]);
};

const report = (fn, problems) => {
  if (!problems.length) {
    console.log(`${fn}: ok`);
  } else {
    console.error(`${fn}: potential problems found`);
    problems.forEach((line) => console.error(`  * ${line}`));
    ++errors;
  }
};

const lddDir = async (leaf) => {
  const files = await ls(leaf);
  for (const fn of files) await ldd(`${leaf}/${fn}`);
};

const main = async () => {
  /* Scan raw vendor libraries or the staged app and its plugins. */
  if (vendorMode) {
    await scanVendor();
  } else {
    await ldd('musikcube');
    await ldd('musikcubed');
    await ldd(`libmusikcore${filetype}`);
    await lddDir('plugins');
    await lddDir('lib');
  }

  if (errors) {
    console.log(
      `\n\n\n    ***** ${errors} problematic files detected! ***** \n\n\n`
    );
    process.exit(1);
  }

  console.log('\neverything looks good!\n');
};

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
