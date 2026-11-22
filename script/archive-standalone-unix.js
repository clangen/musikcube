/* Build, audit, and package the standalone Unix application. */
const { spawnSync } = require('child_process');
const {
  copyFileSync, existsSync, lstatSync, mkdirSync, mkdtempSync, readFileSync, readdirSync,
  realpathSync, renameSync, rmSync, symlinkSync, unlinkSync, writeFileSync,
} = require('fs');
const { join, resolve } = require('path');
const {
  ask, buildArchiveConfiguration, compilerNames, configureRpiArchive, configureZigArchive, takeOption,
} = require('./unix-build-config');

const source = realpathSync(join(__dirname, '..'));
const root = join(source, '..');

function run(program, args, env, capture = false, cwd = source) {
  const result = spawnSync(program, args, {
    cwd,
    env,
    encoding: 'utf8',
    stdio: capture ? ['ignore', 'pipe', 'inherit'] : 'inherit',
  });
  if (result.error) throw result.error;
  if (result.status !== 0) throw new Error(`${program} failed (${result.signal || result.status})`);
  return capture ? result.stdout : undefined;
}

function linkVendor(vendor) {
  const link = join(source, 'vendor');
  try {
    if (!lstatSync(link).isSymbolicLink()) {
      throw new Error('Expected vendor to be a symlink. Move the existing path aside before packaging.');
    }
    unlinkSync(link);
  } catch (error) {
    if (error.code !== 'ENOENT') throw error;
  }
  symlinkSync(vendor, link);
}

function buildApp(config, vendor, clean) {
  const { env, buildDirectory } = config;
  if (clean) {
    console.log('Cleaning the application build');
    if (buildDirectory === source) {
      const cleanup = spawnSync('sh', [join(__dirname, 'clean-nix.sh')], { cwd: source, env, stdio: 'inherit' });
      if (cleanup.error) throw cleanup.error;
    } else {
      rmSync(buildDirectory, { recursive: true, force: true });
      mkdirSync(buildDirectory, { recursive: true });
    }
  } else {
    console.log('Reusing the application build');
  }

  run('bash', [join(__dirname, 'stage-vendor-libraries.sh'), vendor], env, false, buildDirectory);
  if (clean) {
    const args = [
      ...config.cmake,
      '-DCMAKE_BUILD_TYPE=Release',
      '-DCMAKE_EXPORT_COMPILE_COMMANDS=ON',
      '-DBUILD_STANDALONE=true',
      '-DBUILD_TESTING=OFF',
      '-DENABLE_PCH=true',
      ...config.packageCmake,
    ];
    if (buildDirectory !== source) {
      args.push('-DMUSIKCUBE_VENDOR_ROOT=' + vendor, '-DMUSIKCUBE_RUNTIME_OUTPUT_DIRECTORY=' + join(buildDirectory, 'bin'));
    }
    run('cmake', [...args, '-S', source, '-B', buildDirectory], env);
    run('make', [config.jobs], env, false, buildDirectory);
  }

  run('sh', [join(__dirname, 'patch-rpath.sh'), buildDirectory], env, false, buildDirectory);
}

function stageArchive(config, output) {
  const { extension, strip, stripArguments } = config;
  const bin = join(config.buildDirectory, 'bin');
  const files = ['musikcube', 'musikcubed', 'libmusikcore' + extension];
  const lib = readdirSync(join(bin, 'lib'))
    .filter((file) => lstatSync(join(bin, 'lib', file)).isFile())
    .map((file) => join('lib', file));
  const plugins = readdirSync(join(bin, 'plugins'))
    .filter((file) => file.endsWith(extension)).map((file) => join('plugins', file));

  rmSync(output, { recursive: true, force: true });
  for (const directory of ['lib', 'plugins', 'locales', 'themes', 'share/terminfo']) {
    mkdirSync(join(output, directory), { recursive: true });
  }
  for (const directory of ['locales', 'themes']) {
    for (const file of readdirSync(join(bin, directory)).filter((file) => file.endsWith('.json'))) {
      files.push(join(directory, file));
    }
  }
  for (const file of [...files, ...lib, ...plugins]) {
    copyFileSync(join(bin, file), join(output, file));
  }
  run('cp', ['-Rp', join(bin, 'share/terminfo') + '/.', join(output, 'share/terminfo')], config.env);

  if (config.codesign) {
    /* copied dependencies may retain vendor-build paths even when relocation works. */
    run('cmake', ['-DMUSIKCORE_RUNTIME=' + output, '-P',
      join(source, '.cmake/FixupMachORpaths.cmake')], config.env);
  }

  console.log('Stripping staged binaries');
  const binaries = [...files.slice(0, 3), ...lib, ...plugins].map((file) => join(output, file));
  run(strip, [...stripArguments, ...binaries], config.env);
  if (config.codesign) {
    console.log('Signing staged binaries');
    for (const file of binaries) run('codesign', ['--force', '--sign=-', file], config.env);
  }
}

function createPackages(output, archive, { buildDirectory, env }) {
  const staging = mkdtempSync(join(output, '.cpack-'));
  try {
    /* keep CPack from rebuilding and replacing the patched binaries. */
    const config = readFileSync(join(buildDirectory, 'CPackConfig.cmake'), 'utf8');
    const stagedConfig = join(staging, 'CPackConfig.cmake');
    writeFileSync(stagedConfig, config.replace('Unix Makefiles', 'Ninja'));
    run('cpack', ['--config', stagedConfig, '-B', staging,
      '-D', 'CPACK_PRE_BUILD_SCRIPTS=' + join(source, '.cmake', 'StageStandalonePackage.cmake'),
      '-D', 'MUSIKCUBE_STAGED_ARCHIVE=' + archive,
      '-D', 'CPACK_RPM_SPEC_MORE_DEFINE=%global _tmppath ' + staging], env);

    const packages = readdirSync(staging).filter((file) => /\.(deb|rpm)$/.test(file));
    for (const extension of ['.deb', '.rpm']) {
      if (!packages.some((file) => file.endsWith(extension))) {
        throw new Error(`CPack did not generate a ${extension} package.`);
      }
    }
    for (const file of packages) renameSync(join(staging, file), join(output, file));
  } finally {
    rmSync(staging, { recursive: true, force: true });
  }
}

function scanArchive(output, { audit, env }) {
  console.log('Scanning standalone dependencies');
  run(process.execPath, [join(__dirname, 'scan-standalone.js'), output, ...audit], env);
}

function configureRpi(config) {
  config.prepare = (vendor) => configureRpiArchive(config, { root, vendor, run });
  config.cleanBuild = () => true;
  config.buildDirectory = join(source, 'build', 'standalone', config.variant);
}

const archiveConfigurations = new Map([
  ['zig', (config) => Object.assign(config, {
    acceptsSysroot: true,
    prepare: (vendor, sysroot) => configureZigArchive(config, { root, vendor, sysroot, run }),
    cleanBuild: () => true,
  })],
  ['rpi-armv6', configureRpi],
  ['rpi-armv8', configureRpi],
]);

function configuration(compiler) {
  const config = buildArchiveConfiguration(root, compiler);
  Object.assign(config, {
    buildDirectory: source,
    cleanBuild: async () => !/^n$/i.test(await ask('clean and rebuild [y]? ', 'y')),
    scan: scanArchive,
    package: config.generatePackages ? createPackages : undefined,
  });
  archiveConfigurations.get(config.compiler)?.(config);
  return config;
}

async function main() {
  const args = process.argv.slice(2);
  if (!args.length || args.includes('--help') || args.includes('-h')) {
    console.log('Usage: node script/archive-standalone-unix.js <version> [--compiler=<name>] [--sysroot <path>]\n' +
      `Linux compiler profiles: ${compilerNames.join(', ')}. Default uses Zig on Linux and the system compiler on macOS.`);
    if (!args.length) process.exitCode = 1;
    return;
  }
  const compiler = takeOption(args, '--compiler');
  const sysrootOption = takeOption(args, '--sysroot');
  const [version, ...extra] = args;
  if (!version || extra.length) throw new Error('Expected one archive version.');
  if (!/^[a-zA-Z0-9][a-zA-Z0-9._+-]*$/.test(version)) throw new Error('Invalid archive version: ' + version);

  const config = configuration(compiler);
  const sysroot = sysrootOption ? resolve(sysrootOption) : undefined;
  if (sysroot && !config.acceptsSysroot) throw new Error('--sysroot is supported for native Linux builds.');
  const vendor = join(root, 'vendor-' + config.variant);
  if (!existsSync(join(vendor, 'bin/lib'))) {
    throw new Error(`Vendor libraries missing from ${vendor}. Run build-vendor-libraries.js first.`);
  }

  /* Verify the vendor build before changing the checkout's vendor link. */
  await config.prepare?.(vendor, sysroot);
  mkdirSync(config.env.TMPDIR, { recursive: true });

  if (config.buildDirectory === source) linkVendor(vendor);
  buildApp(config, vendor, await config.cleanBuild());

  /* Scan the staged archive before creating release files. */
  const outName = `musikcube_${version}_${config.osArch}`;
  const destination = join(source, 'dist', version);
  const output = join(destination, outName);
  stageArchive(config, output);
  config.scan(output, config);

  run('tar', ['-cjf', output + '.tar.bz2', '-C', destination, outName], config.env);
  config.package?.(destination, output, config);
  console.log('Standalone archives written to ' + destination);
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
