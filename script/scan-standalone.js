const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const readdir = promisify(require('fs').readdir);

const validLibraries = new Set([
  'linux-vdso.so.1',
  'libstdc++.so.6',
  'libm.so.6',
  'libgcc_s.so.1',
  'libc.so.6',
  'libz.so.1',
  'libdl.so.2',
  '/lib64/ld-linux-x86-64.so.2',
]);

let errors = 0;
const path = 'dist/0.96.14/musikcube_standalone_linux_x86_64_0.96.14';

async function ls(leaf) {
  const output = await readdir(`${path}/${leaf}`);
  const files = output.filter((fn) => fn.indexOf('.so') !== -1);
  return files;
}

async function ldd(fn) {
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
  if (!problems.length) {
    console.log(`${fn}: ok`);
  } else {
    console.log(`${fn}: potential problems found`);
    problems.forEach((line) => console.log(`  * ${line}`));
    ++errors;
  }
}

async function lddDir(leaf) {
  const files = await ls(leaf);
  await Promise.allSettled(files.map((fn) => ldd(`${leaf}/${fn}`)));
}

const main = async () => {
  await ldd('musikcube');
  await ldd('musikcubed');
  await ldd('libmusikcore.so');
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
