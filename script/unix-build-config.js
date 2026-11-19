/* Compiler profiles shared by the Unix vendor and standalone builds. */
const { execFileSync, spawnSync } = require('child_process');
const { existsSync, readdirSync, realpathSync, statSync } = require('fs');
const { join } = require('path');
const { createInterface } = require('readline');
const { glibcVersion, stage } = require('./stage-zig-toolchain');

const mac = process.platform === 'darwin';
const compilerNames = ['default', 'zig', 'rpi-armv6', 'rpi-armv8', 'x86'];
const zigStaticLibraries = ['libncursesw.a', 'libpanelw.a', 'libtinfo.a', 'libev.a'];
const raspberryPi = {
  'rpi-armv6': ['armv6', 'armv6-rpi-linux-gnueabihf'],
  'rpi-armv8': ['armv8', 'armv8-rpi3-linux-gnueabihf'],
};

function ask(question, fallback) {
  const input = createInterface({ input: process.stdin, output: process.stdout });
  return new Promise((resolve) => {
    input.once('close', () => resolve(fallback));
    input.question(question, (answer) => {
      resolve(answer.trim() || fallback);
      input.close();
    });
  });
}

function buildConfiguration(compiler, root, flags = {}) {
  if (!mac && process.platform !== 'linux') {
    throw new Error('Unix builds require Linux or macOS.');
  }
  if (!compilerNames.includes(compiler)) {
    throw new Error(`Unknown compiler "${compiler}". Choose from ${compilerNames.join(', ')}.`);
  }

  const capture = { encoding: 'utf8', stdio: ['ignore', 'pipe', 'inherit'] };
  const arch = execFileSync('uname', ['-m'], capture).trim();
  const cpuCount = mac ? spawnSync('sysctl', ['-n', 'hw.ncpu'], capture) : spawnSync('nproc', [], capture);
  const jobs = cpuCount.status === 0 ? '-j' + cpuCount.stdout.trim() : '-j8';
  const env = { ...process.env, TMPDIR: join(root, '.vendor-cache', 'tmp'), ...flags };
  const config = { compiler, arch, env, jobs, marker: '', variant: arch, audit: [], cmake: [] };

  /* Set the target, vendor directory name, and audit options together. */
  if (compiler === 'zig') {
    if (mac || process.arch !== 'x64') {
      throw new Error('The Zig toolchain currently requires Linux x86-64.');
    }

    config.marker = `zig-glibc${glibcVersion}`;
    config.variant = arch + '-' + config.marker;
    config.audit = ['--glibc-max', glibcVersion];
  } else if (raspberryPi[compiler]) {
    const [arm, toolchain] = raspberryPi[compiler];
    const tools = join('/build', 'x-tools', toolchain);
    config.marker = compiler;
    config.variant = compiler;
    config.rpi = { arm, toolchain, tools, sysroot: join(tools, toolchain, 'sysroot') };
    config.cmake = ['--toolchain', join(__dirname, '..', '.cmake', `RaspberryPiToolchain-${arm}.cmake`)];
    env.PATH = join(tools, 'bin') + ':' + env.PATH;
  } else if (compiler === 'x86') {
    config.marker = 'x86';
  }

  return config;
}

function buildArchiveConfiguration(root) {
  const linux = process.platform === 'linux';
  const cross = process.env.CROSSCOMPILE || '';
  const linuxCompiler = cross.startsWith('rpi-') || cross === 'x86' ? cross : 'zig';
  const config = buildConfiguration(linux ? linuxCompiler : 'default', root);
  const arch = config.rpi?.arm ?? (config.compiler === 'x86' ? 'x86' : config.arch);
  const platform = config.rpi ? 'linux_rpi' : linux ? 'linux' : 'macos';

  Object.assign(config, {
    osArch: platform + '_' + arch,
    extension: linux ? '.so' : '.dylib',
    strip: config.rpi ? join(config.rpi.tools, 'bin/strip') : 'strip',
    stripArguments: mac ? ['-x'] : [],
    codesign: mac,
    generatePackages: linux,
    packageCmake: [],
  });

  if (config.compiler === 'x86') config.variant = 'x86';
  if (config.rpi) config.cmake.push('-DENABLE_PIPEWIRE=false');
  if (config.generatePackages) {
    const debArch = config.rpi ? 'armhf' : arch === 'x86_64' ? 'amd64' : arch;
    config.packageCmake.push('-DGENERATE_DEB=true', '-DPACKAGE_ARCHITECTURE=' + debArch,
      '-DFRIENDLY_ARCHITECTURE_NAME=' + config.osArch, '-DCMAKE_INSTALL_PREFIX=/usr');
  }
  return config;
}

async function stageZigCompiler(config, root) {
  console.log('Staging Zig toolchain');
  config.toolchain = await stage(root);
  /* Give configure and make the same wrappers CMake uses. */
  const { wrappers } = config.toolchain;
  Object.assign(config.env, {
    CC: join(wrappers, 'cc'),
    CXX: join(wrappers, 'c++'),
    AR: join(wrappers, 'ar'),
    RANLIB: join(wrappers, 'ranlib'),
  });
  delete config.env.LD;
}

function resolveZigSysroot(path) {
  const include = join(path, 'usr/include');
  if (!existsSync(include) || !statSync(include).isDirectory()) {
    throw new Error(`Missing Zig sysroot at ${path}. Prepare glibc ${glibcVersion}-compatible dependencies or pass --sysroot.`);
  }
  const sysroot = realpathSync(path);
  if (sysroot === '/') throw new Error('Use a dependency sysroot instead of the host filesystem root.');
  return sysroot;
}

async function configureZigArchive(config, { root, vendor, sysroot, run }) {
  sysroot = resolveZigSysroot(sysroot ?? join(vendor, 'sysroot'));

  if (!existsSync(join(vendor, `.config-${config.arch}-${config.marker}`))) {
    throw new Error(`Expected a Zig build marker in ${vendor}. Rebuild the vendors with --compiler zig.`);
  }

  const { env } = config;
  console.log('Checking Zig vendor libraries in ' + vendor);
  run(process.execPath, [join(__dirname, 'scan-standalone.js'), '--vendor',
    join(vendor, 'bin/lib'), ...config.audit], env);

  await stageZigCompiler(config, root);
  const zigClang = run(env.CC, ['--version'], env, true).match(/clang version [0-9.]+/)?.[0];
  if (!zigClang) throw new Error('Could not identify the staged Zig compiler.');

  /* The marker records the selected build. ELF metadata catches mixed compiler outputs. */
  function checkCompiler(directory) {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = join(directory, entry.name);
      if (entry.isDirectory()) {
        checkCompiler(path);
      } else if (entry.isFile() && /\.so(?:\.[0-9]+)*$/.test(entry.name)) {
        const info = run('readelf', ['-p', '.comment', path], { ...env, LC_ALL: 'C' }, true);
        if (!info.includes(zigClang) || !info.includes('Linker: LLD')) {
          throw new Error(`Unexpected compiler metadata in ${path}. Rebuild the vendors with --compiler zig.`);
        }
      }
    }
  }
  checkCompiler(join(vendor, 'bin/lib'));

  for (const library of zigStaticLibraries) {
    const path = join(vendor, 'sysroot/usr/lib', library);
    if (!existsSync(path)) {
      throw new Error(`Missing ${path}. Rebuild the vendors with --compiler zig.`);
    }
    const info = run('readelf', ['-p', '.comment', path], { ...env, LC_ALL: 'C' }, true);
    const members = info.split(/^File: /m).slice(1);
    if (!members.length || members.some((member) => !member.includes(zigClang))) {
      throw new Error(`Unexpected compiler metadata in ${path}. Rebuild the vendors with --compiler zig.`);
    }
  }

  Object.assign(env, {
    MUSIKCUBE_RUNTIME_LIB_DIRS: ['usr/lib/x86_64-linux-gnu', 'lib/x86_64-linux-gnu', 'usr/lib', 'lib']
      .map((directory) => join(sysroot, directory)).join(':'),
    MUSIKCUBE_REQUIRED_RUNTIME_LIBS: 'libz.so.1',
    MUSIKCUBE_OPTIONAL_RUNTIME_LIBS: '',
    MUSIKCUBE_TERMINFO_DIR: join(vendor, 'sysroot/usr/share/terminfo'),
  });
  config.cmake.push(
    '--toolchain', join(__dirname, '..', '.cmake', 'ZigLinuxToolchain.cmake'),
    '-DMUSIKCUBE_DEPENDENCY_ROOT=' + sysroot,
    '-DMUSIKCUBE_ZIG_WRAPPERS=' + config.toolchain.wrappers,
  );
}

module.exports = {
  ask, buildArchiveConfiguration, buildConfiguration, compilerNames, configureZigArchive,
  glibcVersion, resolveZigSysroot, stageZigCompiler,
};
