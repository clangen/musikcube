const { promisify } = require('util');
const exec = promisify(require('child_process').exec);
const fs = require('fs');
const path = require('path');

const TOOLCHAINS = ['armv6-rpi-linux-gnueabihf', 'armv8-rpi3-linux-gnueabihf'];

const run = async (command) => {
  await exec(command, { maxBuffer: 1024 * 1024 * 8 }); /* 8mb */
};

const install = async (toolchain) => {
  const archiveFn = `x-tools-${toolchain}.tar.xz`;
  const toolsRootDir = `x-tools/${toolchain}`;
  const sysrootDir = `${toolsRootDir}/${toolchain}`;
  const downloadUrl = `https://github.com/tttapa/docker-arm-cross-toolchain/releases/download/0.1.2/${archiveFn}`;
  console.log(' * downloading...');
  await run(`wget ${downloadUrl}`);
  console.log(' * extracting...');
  await run(`tar xvf ${archiveFn}`);
  console.log(' * updating permissions...');
  await run(`chmod u+w ${sysrootDir}`);
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

main();
