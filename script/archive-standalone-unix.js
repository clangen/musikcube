/* Build, audit, and package the standalone Unix application. */
const { spawnSync } = require('child_process');
const {
  copyFileSync, existsSync, lstatSync, mkdirSync, mkdtempSync, readFileSync, readdirSync,
  realpathSync, renameSync, rmSync, symlinkSync, unlinkSync, writeFileSync,
} = require('fs');
const { join, resolve } = require('path');
const { ask, buildArchiveConfiguration, configureZigArchive } = require('./unix-build-config');

const source = realpathSync(join(__dirname, '..'));
const root = join(source, '..');

function run(program, args, env, capture = false) {
  const result = spawnSync(program, args, {
    cwd: source,
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

function buildApp(config, clean) {
  const { env } = config;
  if (clean) {
    console.log('Cleaning the application build');
    /* The cleanup script tolerates missing files from an initial build. */
    const cleanup = spawnSync('sh', [join(__dirname, 'clean-nix.sh')], { cwd: source, env, stdio: 'inherit' });
    if (cleanup.error) throw cleanup.error;
  } else {
    console.log('Reusing the application build');
  }

  run('bash', [join(__dirname, 'stage-vendor-libraries.sh')], env);
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
    run('cmake', [...args, '.'], env);
    run('make', [config.jobs], env);
  }

  run('sh', [join(__dirname, 'patch-rpath.sh'), source], env);
}

function stageArchive(config, output) {
  const { extension, strip, stripArguments } = config;
  const bin = join(source, 'bin');
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

  console.log('Stripping staged binaries');
  const binaries = [...files.slice(0, 3), ...lib, ...plugins].map((file) => join(output, file));
  run(strip, [...stripArguments, ...binaries], config.env);
  if (config.codesign) {
    console.log('Signing staged binaries');
    for (const file of binaries) run('codesign', ['--force', '--sign=-', file], config.env);
  }
}

function createPackages(output, env) {
  const staging = mkdtempSync(join(output, '.cpack-'));
  try {
    /* Keep CPack from rebuilding and replacing the stripped, patched binaries. */
    const config = readFileSync(join(source, 'CPackConfig.cmake'), 'utf8');
    const stagedConfig = join(staging, 'CPackConfig.cmake');
    writeFileSync(stagedConfig, config.replace('Unix Makefiles', 'Ninja'));
    run('cpack', ['--config', stagedConfig, '-B', staging], env);

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
  config.scan = () => console.log('Skipping the runtime dependency scan for the Raspberry Pi cross-build');
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

function configuration() {
  const config = buildArchiveConfiguration(root);
  Object.assign(config, {
    cleanBuild: async () => !/^n$/i.test(await ask('clean and rebuild [y]? ', 'y')),
    scan: scanArchive,
    package: config.generatePackages ? createPackages : undefined,
  });
  archiveConfigurations.get(config.compiler)?.(config);
  return config;
}

async function main() {
  const [version, ...args] = process.argv.slice(2);
  if (!version || version === '--help' || version === '-h') {
    console.log('Usage: node script/archive-standalone-unix.js <version> [--sysroot <path>]');
    if (!version) process.exitCode = 1;
    return;
  }
  if (!/^[a-zA-Z0-9][a-zA-Z0-9._+-]*$/.test(version)) throw new Error('Invalid archive version: ' + version);
  if (args.length && (args.length !== 2 || args[0] !== '--sysroot' || !args[1] || args[1].startsWith('--'))) {
    throw new Error('Expected --sysroot <path>.');
  }

  const config = configuration();
  const sysroot = args.length ? resolve(args[1]) : undefined;
  if (sysroot && !config.acceptsSysroot) throw new Error('--sysroot is supported for native Linux builds.');
  const vendor = join(root, 'vendor-' + config.variant);
  if (!existsSync(join(vendor, 'bin/lib'))) {
    throw new Error(`Vendor libraries missing from ${vendor}. Run build-vendor-libraries.js first.`);
  }

  /* Verify the Zig vendor build before changing the checkout's vendor link. */
  await config.prepare?.(vendor, sysroot);
  mkdirSync(config.env.TMPDIR, { recursive: true });

  linkVendor(vendor);
  buildApp(config, await config.cleanBuild());

  /* Scan the staged archive before creating release files. */
  const outName = `musikcube_${version}_${config.osArch}`;
  const destination = join(source, 'dist', version);
  const output = join(destination, outName);
  stageArchive(config, output);
  config.scan(output, config);

  run('tar', ['-cjf', output + '.tar.bz2', '-C', destination, outName], config.env);
  config.package?.(destination, config.env);
  console.log('Standalone archives written to ' + destination);
}

main().catch((error) => {
  console.error(error.message);
  process.exitCode = 1;
});
