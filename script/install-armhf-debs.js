const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const fs = require('fs');

/**
 * This script will download and extract all transitive dependencies
 * for the packages specified by PACKAGE_NAMES. This is used to populate
 * a cross-compile sysroot with the libraries required to compile the
 * app itself.
 */
const PACKAGE_NAMES = [
    'libncurses-dev',
    'libncursesw6',
    'libopus-dev',
    'libopus0',
    'libtinfo6',
    'libvorbis-dev',
    'libvorbis0a',
    'libvorbisenc2',
];

const EXCLUDE = [
    'libc6',
    'libgcc-s1',
    'libcrypt1',
    'gcc-10-base',
    'gcc-13-base',
];

const CLEANUP_FILES = [
    'control.tar.xz',
    'control.tar.zst',
    'data.tar.xz',
    'data.tar.zst',
    'debian-binary',
];

const getPackageDependencies = async (packageName) => {
    console.log('scanning', packageName);
    const rawOutput = (await exec(`apt-cache depends --recurse --no-recommends --no-suggests --no-conflicts --no-breaks --no-replaces --no-enhances ${packageName}`)).stdout.split('\n');
    const packages = rawOutput
        .filter(e => e.indexOf('Depends:') >= 0)
        .map(e => e.replace('Depends:', '').trim())
        .filter(e => !(e.startsWith('Pre ') || e.startsWith('<') || e.startsWith('|')));
    return packages;
};

const getPackageDownloadUrls = async (packages) => {
    const rawOutput = (await exec(`apt download --print-uris ${packages.join(' ')}`)).stdout.split('\n');
    const downloadUrls = rawOutput
        .filter(e => e.trim().length > 0)
        .map(e => e.split(' ')[0].trim().replace(/'/g, ''));
    return downloadUrls;
};

const downloadAndExtract = async (downloadUrls) => {
    for (let i = 0; i < downloadUrls.length; i++) {
        const fn = decodeURIComponent(downloadUrls[i].split('/').pop());
        if (!fs.existsSync(fn)) {
            console.log('downloading', downloadUrls[i]);
            await exec(`wget ${downloadUrls[i]}`);
        }
        console.log('extracting', downloadUrls[i]);
        await exec(`ar x ./${fn}`);
        if (fs.existsSync('data.tar.zst')) {
            await exec(`tar --use-compress-program=unzstd -xvf data.tar.zst`);
        }
        else if (fs.existsSync('data.tar.xz')) {
            await exec(`tar -xvf data.tar.xz`);
        }
        else {
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

const rmDebs = async () => {
    try {
        console.log('cleaning up downloads');
        await exec('rm *.deb');
    }
    catch (e) {
        /* nothing */
    }
};

const main = async () => {
    await rmDebs();
    let dependencies = [];
    for (let i = 0; i < PACKAGE_NAMES.length; i++) {
        dependencies = [...dependencies, ...(await getPackageDependencies(PACKAGE_NAMES[i]))];
    }
    const deduped = new Set([...PACKAGE_NAMES, ...dependencies]);
    for (let i = 0; i < EXCLUDE.length; i++) {
        deduped.delete(EXCLUDE[i]);
    }
    console.log('dependency list:', Array.from(deduped).sort());
    const downloadUrls = await getPackageDownloadUrls(Array.from(deduped));
    await downloadAndExtract(downloadUrls);
    console.log('download urls:', downloadUrls);
    await rmDebs();
    await exec('tar cvf sysroot.tar .');
};

main();
