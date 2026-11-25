// Build and stage the Unix dependencies used by archive-standalone-unix.js.
const { spawnSync } = require('child_process');
const {
  closeSync,
  copyFileSync,
  existsSync,
  fstatSync,
  mkdirSync,
  mkdtempSync,
  openSync,
  readlinkSync,
  readSync,
  readdirSync,
  realpathSync,
  renameSync,
  rmSync,
  symlinkSync,
  unlinkSync,
  writeFileSync,
} = require('fs');
const { basename, dirname, isAbsolute, join, relative, resolve } = require('path');
const {
  ask, buildConfiguration, compilerNames, glibcVersion, resolveZigSysroot,
  stageRpiCompiler, stageZigCompiler, takeOption,
} = require('./unix-build-config');

const ffmpeg = require('./vendor-packages/unix/ffmpeg');
const debianPluginDependencies = require('./vendor-packages/unix/debian-plugin-dependencies');
const zlibRecipe = require('./vendor-packages/unix/zlib');
const staticSysrootRecipes = [
  require('./vendor-packages/unix/ncurses'),
  require('./vendor-packages/unix/libev'),
];
const commonRecipes = [
  /* curl links against the OpenSSL built in this tree. */
  require('./vendor-packages/unix/openssl'),
  require('./vendor-packages/unix/curl'),
  require('./vendor-packages/unix/libopenmpt'),
  require('./vendor-packages/unix/libmicrohttpd'),
  require('./vendor-packages/unix/lame'),
  require('./vendor-packages/unix/taglib'),
  require('./vendor-packages/unix/gme'),
];

const mac = process.platform === 'darwin';
const projectm = require('./vendor-packages/unix/projectm');
if (!mac) commonRecipes.push(projectm);
const buildDirectory = process.cwd();
const cache = join(buildDirectory, '.vendor-cache', 'downloads');
const scratch = join(buildDirectory, '.vendor-cache', 'tmp');
const verbose = process.argv.includes('--verbose', 2);

function run(program, args, { capture = false, ...options } = {}) {
  const logDir = !capture && !verbose ? mkdtempSync(join(scratch, 'build-log-')) : null;
  let log;

  try {
    if (logDir) log = openSync(join(logDir, 'output'), 'w+');

    const stdio = capture ? ['ignore', 'pipe', 'inherit']
      : logDir ? ['inherit', log, log] : 'inherit';
    const result = spawnSync(program, args, {
      encoding: 'utf8',
      stdio,
      env: { ...process.env, TMPDIR: scratch },
      ...options,
    });

    if (result.error) throw result.error;
    if (result.status !== 0) {
      if (logDir) {
        const size = fstatSync(log).size;
        const tail = Buffer.alloc(Math.min(size, 64 * 1024));
        const bytesRead = readSync(log, tail, 0, tail.length, size - tail.length);
        if (bytesRead) console.error(tail.toString('utf8', 0, bytesRead).trimEnd());
      }

      throw new Error(`${program} ${args.join(' ')} failed (${result.signal || result.status})`);
    }

    return capture ? result.stdout.trim() : undefined;
  } finally {
    if (log !== undefined) closeSync(log);
    if (logDir) rmSync(logDir, { recursive: true, force: true });
  }
}

function configureStaticDependencies(config, out, sysroot) {
  const { env } = config;

  /* Build zlib against the same glibc baseline as its consumers. */
  const zlib = join(out, '..', '..', 'static-zlib');
  const destination = join(out, '..', 'sysroot');
  Object.assign(config, {
    stageSysroot: sysroot ? () => stageZigSysroot(sysroot, destination) : () => {},
    stagePluginDependencies: sysroot ? undefined : (work) => {
      debianPluginDependencies.stage({ destination, download, run, work, target: config.rpi?.manifest });
      debianPluginDependencies.stage({ destination, download, run, work,
        target: config.rpi ? 'projectm-armhf' : 'projectm-amd64' });
      relocateSysrootLinks(destination);
    },
    sourceSysroot: sysroot ? undefined : destination,
    staticSysroot: destination,
    prerequisites: [zlibRecipe, ...staticSysrootRecipes],
    auditDirectories: sysroot ? [] : [join(destination, 'usr/lib')],
    zlib,
    curl: ['--with-zlib=' + zlib],
    /* FFmpeg selects its compiler and archiver through configure flags. */
    ffmpeg: [
      '--cc=' + env.CC,
      '--cxx=' + env.CXX,
      '--ld=' + env.CC,
      '--ar=' + env.AR,
      '--ranlib=' + env.RANLIB,
    ],
    cmake: [
      '-DCMAKE_AR=' + env.AR,
      '-DCMAKE_RANLIB=' + env.RANLIB,
      '-DZLIB_ROOT=' + zlib,
      '-DZLIB_USE_STATIC_LIBS=ON',
    ],
    makefileEnv: {
      ...env,
      CPPFLAGS: [env.CPPFLAGS, '-I' + join(zlib, 'include')].filter(Boolean).join(' '),
      LDFLAGS: [env.LDFLAGS, '-L' + join(zlib, 'lib')].filter(Boolean).join(' '),
      PKG_CONFIG_PATH: '',
      PKG_CONFIG_LIBDIR: join(zlib, 'lib', 'pkgconfig'),
      PKG_CONFIG_SYSROOT_DIR: '',
    },
  });
}

async function configureZig(config, out, sysroot) {
  await stageZigCompiler(config, buildDirectory);
  configureStaticDependencies(config, out, sysroot);
  config.openmpt = ['CONFIG=clang', 'CC=' + config.env.CC, 'CXX=' + config.env.CXX,
    'LD=' + config.env.CXX, 'AR=' + config.env.AR];
}

async function configureRpi(config, out) {
  await stageRpiCompiler(config, buildDirectory);
  const { env, toolchain } = config;
  /* keep the newer compiler runtime private to each shared library. */
  env.CXXFLAGS += ' -static-libstdc++ -fvisibility-inlines-hidden';
  env.LDFLAGS = [env.LDFLAGS, '-Wl,--exclude-libs,libstdc++.a:libatomic.a'].filter(Boolean).join(' ');
  configureStaticDependencies(config, out);
  config.openssl = ['-l' + config.rpi.atomic, 'linux-generic32'];
  config.configure = ['--build=x86_64-pc-linux-gnu', '--host=arm-linux-gnueabihf'];
  config.ffmpeg.push('--arch=arm', '--target-os=linux', '--enable-cross-compile',
    '--strip=' + config.env.STRIP);
  /* opus uses rounding intrinsics missing from GCC's 32-bit ARM headers. */
  config.opus = ['--disable-intrinsics'];
  if (config.arch === 'armv6') {
    config.ffmpeg.push('--disable-neon', '--disable-vfpv3');
    config.opus.push('--disable-asm');
  }
  config.cmake.push('--toolchain', toolchain.cmake);
  config.openmpt = ['CONFIG=gcc', 'CC=' + env.CC, 'CXX=' + env.CXX, 'LD=' + env.CXX, 'AR=' + env.AR,
    'LDFLAGS=' + [config.makefileEnv.LDFLAGS, '-static-libstdc++'].filter(Boolean).join(' ')];
}

function configureX86(config) {
  config.openssl = ['linux-x86'];
  config.env.CFLAGS += ' -I/usr/include/i386-linux-gnu/';
  config.env.CXXFLAGS += ' -I/usr/include/i386-linux-gnu/';
}

const vendorConfigurations = new Map([
  ['zig', configureZig],
  ['rpi-armv6', configureRpi],
  ['rpi-armv8', configureRpi],
  ['x86', configureX86],
]);

async function configuration(out, compiler, sysroot) {
  /* Vendor release flags also keep Zig from enabling UBSan at -O0. */
  const config = buildConfiguration(compiler, buildDirectory, {
    CFLAGS: '-fPIC -O2',
    CXXFLAGS: '-fPIC -O2 -std=c++17',
  });
  const { arch, env } = config;
  Object.assign(config, {
    prerequisites: [],
    auditDirectories: [],
    stageSysroot: () => {},
    configure: [],
    curl: [],
    ffmpeg: [],
    openmpt: [],
    openssl: [mac ? `darwin64-${arch}-cc` : `linux-${arch}`],
    makefileEnv: env,
  });

  await vendorConfigurations.get(compiler)?.(config, out, sysroot);
  return config;
}

function printConfiguration(config) {
  const { env, makefileEnv, prerequisites, ...options } = config;
  /* Report build settings without dumping unrelated inherited variables. */
  const tools = ['CC', 'CXX', 'CPP', 'LD', 'AR', 'RANLIB', 'NM', 'STRIP', 'PKG_CONFIG'];
  const flags = ['CFLAGS', 'CXXFLAGS', 'CPPFLAGS', 'LDFLAGS', 'LIBS', 'ARFLAGS'];
  const variables = [
    'CPATH', 'C_INCLUDE_PATH', 'CPLUS_INCLUDE_PATH', 'LIBRARY_PATH', 'LD_LIBRARY_PATH',
    'PKG_CONFIG_PATH', 'PKG_CONFIG_LIBDIR', 'PKG_CONFIG_SYSROOT_DIR',
    'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PREFIX_PATH', 'PATH', 'TMPDIR',
  ];
  const environment = Object.fromEntries([
    ...tools.map((name) => [name, env[name] ?? '(build system default)']),
    ...flags.map((name) => [name, env[name] ?? '']),
    ...variables.filter((name) => env[name] !== undefined).map((name) => [name, env[name]]),
  ]);
  const makefileEnvironmentOverrides = Object.fromEntries(
    Object.entries(makefileEnv).filter(([name, value]) => value !== env[name])
  );

  console.log('Build configuration');
  console.log(JSON.stringify({
    environment,
    makefileEnvironmentOverrides,
    ...options,
    prerequisitePackages: prerequisites.flatMap((recipe) => Object.keys(recipe.packages)),
  }, null, 2));
}

function stageZigSysroot(source, destination) {
  console.log('Staging Zig sysroot from ' + source);
  mkdirSync(destination);
  run('cp', ['-Rp', source + '/.', destination]);
  relocateSysrootLinks(destination);
}

function relocateSysrootLinks(destination) {
  /* Keep absolute package symlinks inside the staged sysroot. */
  function relocateLinks(directory) {
    for (const entry of readdirSync(directory, { withFileTypes: true })) {
      const path = join(directory, entry.name);
      if (entry.isDirectory()) {
        relocateLinks(path);
      } else if (entry.isSymbolicLink()) {
        const target = readlinkSync(path);
        if (isAbsolute(target)) {
          unlinkSync(path);
          symlinkSync(relative(dirname(path), join(destination, resolve(target))), path);
        }
      }
    }
  }
  relocateLinks(destination);
}

function download(url, file) {
  const archive = join(cache, file);
  if (existsSync(archive)) return archive;

  console.log('Downloading ' + file);
  mkdirSync(cache, { recursive: true });
  const temp = mkdtempSync(join(cache, 'download-'));
  const pending = join(temp, file);
  try {
    run('wget', ['-O', pending, url]);
    renameSync(pending, archive);
  } finally {
    rmSync(temp, { recursive: true, force: true });
  }
  return archive;
}

function fetchPackages(work, recipes) {
  const packages = Object.assign({}, ...recipes.map((recipe) => recipe.packages));
  const sources = {};

  for (const [name, [directory, file, url]] of Object.entries(packages)) {
    const archive = download(url, file);
    console.log('Extracting ' + file);
    copyFileSync(archive, join(work, file));
    run('tar', [file.endsWith('.bz2') ? '-xjf' : '-xzf', file], { cwd: work });
    sources[name] = join(work, directory);
  }

  return sources;
}

function cleanLibraries(lib) {
  for (const file of readdirSync(lib)) {
    if ((file.endsWith('.a') && !['libprojectM-4.a', 'libSDL2.a'].includes(file)) || file.endsWith('.la')) rmSync(join(lib, file));
  }

  if (mac) {
    // Keep the ABI version in the filename used by the release staging script.
    for (const [name, abi] of [['avcodec', 62], ['avformat', 62], ['avutil', 60], ['swresample', 6]]) {
      const leaf = `lib${name}-musikcube`;
      const versioned = `${leaf}.${abi}.dylib`;
      const dylib = join(lib, versioned);
      const installed = realpathSync(dylib);
      if (installed !== dylib && basename(installed) !== versioned) {
        renameSync(installed, dylib);
      }
      rmSync(join(lib, leaf + '.dylib'));
      symlinkSync(versioned, join(lib, leaf + '.dylib'));
    }
  }
}

async function main() {
  const args = process.argv.slice(2).filter((arg) => arg !== '--verbose');

  if (args.includes('--help') || args.includes('-h')) {
    console.log('Usage: node script/build-vendor-libraries.js [--verbose] [--compiler=<name>] [--sysroot <path>]\n' +
      'Run from the standalone build directory.\n' +
      '  --verbose   Stream build command output\n' +
      `  --compiler  ${compilerNames.join(', ')}\n` +
      `              zig targets Linux x86-64 with glibc ${glibcVersion}\n` +
      '              Prompts when omitted, with default as the suggested choice.\n' +
      '  --sysroot   Use a prepared Zig sysroot instead of building one');
    return;
  }

  const selectedCompiler = takeOption(args, '--compiler');
  let sysroot = takeOption(args, '--sysroot');

  if (args.length) throw new Error('Unknown Unix vendor build argument: ' + args[0]);
  if (!mac && process.platform !== 'linux') throw new Error('Unix vendor builds require Linux or macOS.');

  /* Resolve the compiler profile before creating the work directory. */
  const compiler = selectedCompiler ?? await ask(`Compiler (${compilerNames.join(', ')}) [default]: `, 'default');
  if (sysroot && compiler !== 'zig') throw new Error('--sysroot requires --compiler zig.');
  if (sysroot) sysroot = resolveZigSysroot(sysroot);
  mkdirSync(scratch, { recursive: true });
  const work = mkdtempSync(join(buildDirectory, '.vendor-build-'));

  try {
    const vendor = join(work, 'vendor');
    const out = join(vendor, 'bin');
    const lib = join(out, 'lib');
    const config = await configuration(out, compiler, sysroot);
    const destination = join(buildDirectory, 'vendor-' + config.variant);

    mkdirSync(vendor);
    writeFileSync(join(vendor, `.config-${config.arch}-${config.marker}`), '');

    console.log(`Building vendors for ${config.variant} (${config.jobs})`);
    printConfiguration(config);

    config.stageSysroot();

    /* Build the static prerequisites before their consumers. */
    const recipes = [ffmpeg, ...config.prerequisites, ...commonRecipes];
    const sources = fetchPackages(vendor, recipes);
    for (const recipe of recipes) {
      if (recipe !== projectm) recipe.build({ sources, out, config, run });
    }
    /* Normalize the runtime libraries before the dependency audit. */
    cleanLibraries(lib);
    run(process.execPath, [join(__dirname, 'relink-dynamic-libraries.js'), 'bin/lib'], { cwd: vendor });

    console.log('Checking vendor library dependencies');
    for (const directory of [lib, ...config.auditDirectories]) {
      run(process.execPath, [
        join(__dirname, 'scan-standalone.js'),
        '--vendor',
        directory,
        ...config.audit,
      ], { cwd: vendor });
    }

    /* These linker inputs are supplied by the host at runtime. */
    config.stagePluginDependencies?.(work);
    if (!mac) projectm.build({ sources, out, config, run });

    /* Keep the previous vendor build until the replacement is in place. */
    const previous = join(work, 'previous');
    const replacing = existsSync(destination);
    if (replacing) renameSync(destination, previous);
    try {
      renameSync(vendor, destination);
    } catch (error) {
      if (replacing) renameSync(previous, destination);
      throw error;
    }

    console.log('Vendor libraries installed in ' + destination);
  } finally {
    rmSync(work, { recursive: true, force: true });
  }
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
