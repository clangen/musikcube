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
const exec = promisify(require('child_process').exec);
const readdir = promisify(require('fs').readdir);

const mac = process.platform === 'darwin';

const filetype = mac ? '.dylib' : '.so';

/* these libraries will always have dynamic dependencies, so ignore */
const ignoredLibraries = mac
  ? new Set([])
  : new Set([
    'libalsaout.so',
    'libmpris.so',
    'libpipewireout.so',
    'libpulseout.so',
    'libsndioout.so',
  ]);

/* we assume these dependencies will always be available on the user's
target machine. */
const validLibraries = mac
  ? new Set([
    '/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit',
    '/System/Library/Frameworks/ApplicationServices.framework/Versions/A/ApplicationServices',
    '/System/Library/Frameworks/AudioToolbox.framework/Versions/A/AudioToolbox',
    '/System/Library/Frameworks/Carbon.framework/Versions/A/Carbon',
    '/System/Library/Frameworks/CoreAudio.framework/Versions/A/CoreAudio',
    '/System/Library/Frameworks/CoreFoundation.framework/Versions/A/CoreFoundation',
    '/System/Library/Frameworks/CoreGraphics.framework/Versions/A/CoreGraphics',
    '/System/Library/Frameworks/CoreMedia.framework/Versions/A/CoreMedia',
    '/System/Library/Frameworks/CoreServices.framework/Versions/A/CoreServices',
    '/System/Library/Frameworks/CoreVideo.framework/Versions/A/CoreVideo',
    '/System/Library/Frameworks/Foundation.framework/Versions/C/Foundation',
    '/System/Library/Frameworks/SystemConfiguration.framework/Versions/A/SystemConfiguration',
    '/usr/lib/libc++.1.dylib',
    '/usr/lib/libobjc.A.dylib',
    '/usr/lib/libSystem.B.dylib',
    '/usr/lib/libz.1.dylib',
    '/opt/homebrew/opt/portaudio/lib/libportaudio.2.dylib',
    '/usr/local/opt/portaudio/lib/libportaudio.2.dylib',
  ])
  : new Set([
    '/lib/ld-linux-armhf.so.3',
    '/lib64/ld-linux-x86-64.so.2',
    '/usr/lib/arm-linux-gnueabihf/libarmmem-${PLATFORM}.so',
    '/usr/lib/arm-linux-gnueabi/libarmmem-${PLATFORM}.so',
    'libc.so.6',
    'libdl.so.2',
    'libgcc_s.so.1',
    'libm.so.6',
    'libpthread.so.0',
    'librt.so.1',
    'libstdc++.so.6',
    'libz.so.1',
    'linux-vdso.so.1',
    'libportaudio.so.2',
    'libasound.so.2',
    'libjack.so.0',
    'libdb-5.3.so',
  ]);

let errors = 0;

const path = process.argv[2];

if (!path) {
  console.log('\n\nusage: node scan-standalone.js <path>\n\n');
  process.exit(1);
}

const ls = async (leaf) => {
  const output = await readdir(`${path}/${leaf}`);
  const files = output
    .filter((fn) => fn.indexOf(filetype) !== -1)
    .filter((fn) => !ignoredLibraries.has(fn));
  return files;
};

const lddLinux = async (fn) => {
  const output = await exec(`ldd ${path}/${fn}`);
  const problems = output.stdout
    .split('\n')
    .map((line) => line.trim())
    /* libraries resolved properly will exist in the current path; exclude */
    .filter((line) => !!line && line.indexOf(path) === -1)
    .filter((line) => line !== 'statically linked')
    /* libraries are formatted as "<name> [=> <path>] <(offset)>". if there's
    a fat arrow, just grab everything to the left of it. then, if there's
    still an offset, strip that as well */
    .map((line) => line.split(' => ')[0].split(' ')[0].trim())
    .filter((line) => !validLibraries.has(line));
  return problems;
};

const lddMac = async (fn) => {
  const output = await exec(`otool -L ${path}/${fn}`);
  const problems = output.stdout
    .split('\n')
    .map((line) => line.trim())
    /* we want all of the libraries we compile ourself to be prefixed
    with the value `@rpath/`. */
    .filter((line) => !!line && line.indexOf('@rpath/') === -1)
    /* the first line of output is the path to the library, exclude it */
    .slice(1)
    /* libraries are formatted as "<name> (compatibility ...)", so split
    and take the name */
    .map((line) => line.split(' (compatibility')[0].trim())
    .filter((line) => !validLibraries.has(line));
  return problems;
};

const ldd = async (fn) => {
  const problems = mac ? await lddMac(fn) : await lddLinux(fn);
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
  await Promise.allSettled(files.map((fn) => ldd(`${leaf}/${fn}`)));
};

const main = async () => {
  await ldd('musikcube');
  await ldd('musikcubed');
  await ldd(`libmusikcore${filetype}`);
  await lddDir('plugins');
  await lddDir('lib');

  if (errors) {
    console.log(
      `\n\n\n    ***** ${errors} problematic files detected! ***** \n\n\n`
    );
    process.exit(1);
  }

  console.log('\neverything looks good!\n');
};

main();
