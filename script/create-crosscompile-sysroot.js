const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const fs = require('fs');
const path = require('path');

/**
 * This script will download and extract all transitive dependencies
 * for the packages specified by PACKAGE_NAMES. This is used to populate
 * a cross-compile sysroot with the libraries required to compile the
 * app itself.
 */
const PACKAGE_NAMES = [
  'libasound2-dev',
  'libev-dev',
  'libncurses-dev',
  'libopus-dev',
  'libportaudio-dev',
  'libpulse-dev',
  'libsndio-dev',
  'libsystemd-dev',
  'libvorbis-dev',
  'zlib1g-dev',
];

const EXCLUDE = [
  'debconf',
  'dpkg',
  'gcc-10-base',
  'gcc-13-base',
  'gcc-8-base',
  'install-info',
  'libc-dev-bin',
  'libc6-dev',
  'libc6',
  'libcrypt1',
  'libdebian-installer4',
  'libdpkg-perl',
  'libgcc-s1',
  'libgcc1',
  'libselinux1-dev',
  'libselinux1',
  'libstdc++6',
  'linux-libc-dev',
];

const CLEANUP_FILES = [
  'control.tar.gz',
  'control.tar.bz2',
  'control.tar.xz',
  'control.tar.zst',
  'data.tar.gz',
  'data.tar.bz2',
  'data.tar.xz',
  'data.tar.zst',
  'debian-binary',
];

const getPackageDependencies = async (packageName) => {
  console.log('scanning', packageName);
  const rawOutput = (
    await exec(
      `apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts --no-breaks --no-replaces --no-enhances ${packageName}`
    )
  ).stdout.split('\n');
  const packages = rawOutput
    .filter((e) => e.indexOf('Depends:') >= 0)
    .map((e) => e.replace('Depends:', '').trim())
    .filter(
      (e) => !(e.startsWith('Pre ') || e.startsWith('<') || e.startsWith('|'))
    );
  return packages;
};

const getPackageDownloadUrls = async (packages) => {
  const rawOutput = (
    await exec(`apt download --print-uris ${packages.join(' ')}`)
  ).stdout.split('\n');
  const downloadUrls = rawOutput
    .filter((e) => e.trim().length > 0)
    .map((e) => e.split(' ')[0].trim().replace(/'/g, ''));
  return downloadUrls;
};

const downloadAndExtract = async (downloadUrls) => {
  for (let i = 0; i < downloadUrls.length; i++) {
    const fn = decodeURIComponent(downloadUrls[i].split('/').pop());
    console.log('processing', downloadUrls[i]);
    await exec(`wget ${downloadUrls[i]}`);
    await exec(`ar x ./${fn}`);
    if (fs.existsSync('data.tar.zst')) {
      await exec(`tar --use-compress-program=unzstd -xvf data.tar.zst`);
    } else if (fs.existsSync('data.tar.xz')) {
      await exec(`tar -xvf data.tar.xz`);
    } else if (fs.existsSync('data.tar.gz')) {
      await exec(`tar xvfz data.tar.gz`);
    } else if (fs.existsSync('data.tar.bz2')) {
      await exec(`tar xvfj data.tar.bz2`);
    } else {
      console.error('unknown file type');
      process.exit(-1);
    }
    for (let j = 0; j < CLEANUP_FILES.length; j++) {
      if (fs.existsSync(CLEANUP_FILES[j])) {
        await exec(`rm ${CLEANUP_FILES[j]}`);
      }
    }
  }
};

const convertAbsoluteToRelativeSymlinks = async () => {
  const root = process.cwd();
  const symlinks = (await exec('find . -type l')).stdout
    .split('\n')
    .filter((e) => e.length > 0);
  for (let i = 0; i < symlinks.length; i++) {
    const dest = fs.readlinkSync(symlinks[i]);
    if (dest[0] === '/') {
      const absoluteTo = path.join(root, dest);
      const absoluteFrom = path.join(root, symlinks[i]);
      const relative = path.relative(path.dirname(absoluteFrom), absoluteTo);
      console.log(
        `relinking\n * fn:   ${absoluteFrom}\n * from: ${absoluteTo}\n * to:   ${relative}`
      );
      fs.unlinkSync(symlinks[i]);
      fs.symlinkSync(relative, symlinks[i]);
    }
  }
};

const rmDebs = async () => {
  try {
    console.log('cleaning up downloads');
    await exec('rm *.deb');
  } catch (e) {
    /* nothing */
  }
};

const main = async () => {
  await rmDebs();
  let dependencies = [];
  for (let i = 0; i < PACKAGE_NAMES.length; i++) {
    dependencies = [
      ...dependencies,
      ...(await getPackageDependencies(PACKAGE_NAMES[i])),
    ];
  }
  const deduped = new Set([...PACKAGE_NAMES, ...dependencies]);
  for (let i = 0; i < EXCLUDE.length; i++) {
    deduped.delete(EXCLUDE[i]);
  }
  console.log(`resolved ${deduped.size} transitive dependencies`);
  const downloadUrls = await getPackageDownloadUrls(Array.from(deduped));
  await downloadAndExtract(downloadUrls);
  await convertAbsoluteToRelativeSymlinks();
  await rmDebs();
  await exec('tar cvf sysroot.tar .', { maxBuffer: 1024 * 1024 * 8 }); /* 8mb */
};

main();
