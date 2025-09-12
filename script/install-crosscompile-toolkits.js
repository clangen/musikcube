const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const fs = require('fs');

const TOOLCHAINS = ['armv6-rpi-linux-gnueabihf', 'armv8-rpi3-linux-gnueabihf'];
const VERSION = '1.1.0';
const GCC_VERSION = '14'

const run = async (command) => {
  console.log('    > ', command);
  await exec(command, { maxBuffer: 1024 * 1024 * 8 }); /* 8mb */
};

const install = async (toolchain) => {
  const archiveFn = `x-tools-${toolchain}-gcc${GCC_VERSION}.tar.xz`;
  const toolsRootDir = `x-tools/${toolchain}`;
  const sysrootDir = `${toolsRootDir}/${toolchain}/sysroot`;
  const downloadUrl = `https://github.com/tttapa/docker-arm-cross-toolchain/releases/download/${VERSION}/${archiveFn}`;
  console.log(' * downloading...');
  await run(`wget ${downloadUrl}`);
  console.log(' * extracting...');
  await run(`tar xvf ${archiveFn}`);
  console.log(' * updating permissions...');
  await run(`chmod u+w ${sysrootDir} -R`);
  console.log(' * installing sysroot...');
  await run(`tar xvf sysroot.tar --directory=${sysrootDir}`);
  console.log(' * cleaning up...');
  await run(`rm ${archiveFn} 2> /dev/null`);
};

main = async () => {
  for (let i = 0; i < TOOLCHAINS.length; i++) {
    console.log(`installing [${i + 1}/${TOOLCHAINS.length}] ${TOOLCHAINS[i]}`);
    await install(TOOLCHAINS[i]);
  }
};

if (!fs.existsSync('./sysroot.tar')) {
  console.error(
    '[ERROR]: sysroot.tar not found in the current directory, process will not continue'
  );
  process.exit(1);
}

main();
