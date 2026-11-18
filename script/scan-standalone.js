/**
 * We want to statically link as many dependencies as possible to "standalone"
 * executables and plugins to make it as easy as possible to just extract and
 * run the app and daemon without needing to install any additional packages.
 *
 * This script scans binaries produced by the build process to ensure there
 * are no unexpected libraries being dynamically linked by using `ldd` on
 * unix systems, and `otool` on mac.
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
  'libpthread.so.0', 'librt.so.1', 'libstdc++.so.6', 'libz.so.1', 'libatomic.so.1',
];

/* macOS runtime libraries and frameworks allowed in both scans. */
const macRuntimeLibraries = [
  '/usr/lib/libc++.1.dylib', '/usr/lib/libSystem.B.dylib', '/usr/lib/libz.1.dylib',
  '/usr/lib/libiconv.2.dylib', '/usr/lib/libresolv.9.dylib', '/usr/lib/libobjc.A.dylib',
  '/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation',
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
const packagePath = process.argv[vendorMode ? 3 : 2];

if (!packagePath) {
  console.log('\n\nusage: node scan-standalone.js [--vendor] <path>\n\n');
  process.exit(1);
}

const root = resolve(packagePath);

const ls = async (leaf) => {
  const output = await readdir(`${root}/${leaf}`);
  const files = output
    .filter((fn) => fn.indexOf(filetype) !== -1)
    .filter((fn) => !ignoredLibraries.has(fn));
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

const neededMac = async (file) => {
  const { stdout } = await execFile('otool', ['-l', file], { env });
  const names = [...stdout.matchAll(
    /^\s*cmd LC_(?:LOAD|LOAD_WEAK|REEXPORT|LAZY_LOAD|LOAD_UPWARD)_DYLIB\s*\n\s*cmdsize \d+\s*\n\s*name (.+?) \(offset \d+\)/gm
  )].map(([, name]) => name);
  /* Universal binaries can list the same dependency in multiple slices. */
  return [...new Set(names)];
};

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

const lddMac = async (fn) => {
  const needed = await neededMac(join(root, fn));
  return needed.filter((name) => !name.startsWith('@rpath/') && !validLibraries.has(name));
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
      if (vendorSystemLibraries.has(name)) return false;
      if (!mac) return basename(name) !== name || !aliases.has(join(directory, name));
      if (name.startsWith('@rpath/')) return !aliases.has(resolve(directory, name.slice(7)));
      if (name.startsWith('@loader_path/')) return !aliases.has(resolve(dirname(file), name.slice(13)));
      return true;
    });
    report(relative(directory, file), problems);
  }
};

const ldd = async (fn) => {
  const problems = mac ? await lddMac(fn) : await lddLinux(fn);
  report(fn, problems);
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
