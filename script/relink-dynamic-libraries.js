/* this script is used to update all macOS dynamic libraries so they can discover
each other in the app's sandboxed directory; it removes absolute paths and replaces
them with @rpaths. it also ensures symlinks are setup properly. whenever we add
a new third-party dependency we need to update the `libraries` and `symlinks`
mapping so the script knows to process them.

the result of the script will be later validated by the `scan-standalone.js` script
during app build-time. */

const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const fs = require('fs');
const rm = promisify(fs.rm);
const unlink = promisify(fs.unlink);
const symlink = promisify(fs.symlink);

const mac = process.platform === 'darwin';

if (!mac) {
  console.log(`\n\n  no need to relink libraries on '${process.platform}\n\n`);
  process.exit(0);
}

/* these are the libraries we'll scan, and update linked libraries from
absolute paths to "@rpath/filename" */
const libraries = [
  'libavcodec-musikcube.59.dylib',
  'libavformat-musikcube.59.dylib',
  'libavutil-musikcube.57.dylib',
  'libswresample-musikcube.4.dylib',
  'libcrypto.1.1.dylib',
  'libssl.1.1.dylib',
  'libcurl.4.dylib',
  'libmicrohttpd.12.dylib',
  'libmp3lame.0.dylib',
  'libogg.0.dylib',
  'libvorbis.0.dylib',
  'libvorbisenc.2.dylib',
  'libopus.0.dylib',
  'libopenmpt.0.dylib',
];

/* after updating libraries, re-establish symlinks */
const symlinks = [
  ['libavcodec-musikcube.59.dylib', 'libavcodec-musikcube.dylib'],
  ['libavformat-musikcube.59.dylib', 'libavformat-musikcube.dylib'],
  ['libavutil-musikcube.57.dylib', 'libavutil-musikcube.dylib'],
  ['libswresample-musikcube.4.dylib', 'libswresample-musikcube.dylib'],
  ['libcrypto.1.1.dylib', 'libcrypto.dylib'],
  ['libssl.1.1.dylib', 'libssl.dylib'],
  ['libcurl.4.dylib', 'libcurl.dylib'],
  ['libmicrohttpd.12.dylib', 'libmicrohttpd.dylib'],
  ['libmp3lame.0.dylib', 'libmp3lame.dylib'],
  ['libogg.0.dylib', 'libogg.dylib'],
  ['libvorbis.0.dylib', 'libvorbis.dylib'],
  ['libvorbisenc.2.dylib', 'libvorbisenc.dylib'],
  ['libopus.0.dylib', 'libopus.dylib'],
  ['libopenmpt.0.dylib', 'libopenmpt.dylib'],
];

const path = process.argv[2];

if (!path) {
  console.log('\n\nusage: node relink-dynamic-libraries.js <path>\n\n');
  process.exit(1);
}

const run = async (cmd) => {
  console.log(cmd);
  return await exec(cmd);
};

/* scans the specified dylib using `otool`, builds a list of dependencies
that need their absolute paths updated with relative paths, then uses
`install_name_tool` to update the paths */
const relink = async (fn) => {
  const output = await exec(`otool -L ${path}/${fn}`);
  const relink = output.stdout
    .split('\n')
    .map((line) => line.trim())
    .filter((line) => !!line)
    /* the first line of output is the path to the library; ignore. */
    .slice()
    /* libraries are formatted as "<name> (compatibility ...)", so split
    and take the name */
    .map((line) => line.split(' (compatibility')[0].trim())
    /* grab the filename from the path, and see if it's one we want to
    re-link relatively */
    .filter((line) => libraries.indexOf(line.split('/').pop()) !== -1);
  await run(`install_name_tool -id "@rpath/${fn}" ${path}/${fn}`);
  for (let i = 0; i < relink.length; i++) {
    const entry = relink[i];
    const leaf = entry.split('/').pop();
    await run(
      `install_name_tool -change "${entry}" "@rpath/${leaf}" ${path}/${fn}`
    );
  }
  /* changing the id and updating entries sometimes invalidates the code
  signature. remove the old one, and re-sign */
  await run(`codesign --remove-signature ${path}/${fn}`);
  await run(`codesign --sign=- ${path}/${fn}`);
  return relink;
};

const rebuildSymlinks = async () => {
  for (let i = 0; i < symlinks.length; i++) {
    const [src, dst] = symlinks[i];
    console.log('removing symlink:', `${path}/${dst}`);
    try {
      await rm(`${path}/${dst}`);
    } catch (e) {}
    try {
      await unlink(`${path}/${dst}`);
    } catch (e) {}
    console.log('creating symlink:', src, dst);
    await symlink(src, `${path}/${dst}`);
  }
};

const main = async () => {
  try {
    await Promise.allSettled(libraries.map((library) => relink(library)));
    await rebuildSymlinks();
  } catch (e) {
    console.log(`\n\n  failed to relink dynamic libraries!\n\n`);
    console.log(e, '\n\n');
    process.exit(1);
  }
  console.log(`\n\n  finished relinking dynamic libraries!\n\n`);
  process.exit(0);
};

main();
