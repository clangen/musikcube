/* Build the Windows release and optionally package the application and SDK. */
const { spawnSync } = require('node:child_process');
const { copyFileSync, cpSync, existsSync, mkdirSync, readFileSync, readdirSync, rmSync } = require('node:fs');
const { join, resolve } = require('node:path');

const APP_EXECUTABLES = ['musikcube.exe', 'musikcube-cmd.exe'];
const ASSET_EXTENSIONS = { themes: '.json', locales: '.json', fonts: '.ttf' };
const CURL_DLLS = ['libcurl.dll'];
const OPENSSL_DLLS = ['libssl-3-x64.dll', 'libcrypto-3-x64.dll'];
const MILKDROP_DLLS = ['D3DX9_43.dll', 'D3DCompiler_43.dll'];
const MILKDROP_PLUGIN = 'milkdrop2.dll';
const MILKDROP_RESOURCES = 'Milkdrop2';
const PROJECTM_RESOURCES = 'projectM';
const SAMPLE_PLUGINS = ['win32gdivis.dll'];
const MILKDROP_REQUIRED = [
  `plugins/${MILKDROP_PLUGIN}`,
  `plugins/${MILKDROP_RESOURCES}`,
  ...MILKDROP_DLLS,
];

const args = process.argv.slice(2);
const usage = 'Usage: node script/build-and-archive.js --version=<version> [--clean] [--skip-archive] [--with-milkdrop]';
if (args.length === 1 && (args[0] === '--help' || args[0] === '-h')) {
  console.log(usage);
  process.exit(0);
}
let version;
let withMilkdrop = false;
let clean = false;
let skipArchive = false;
let invalid = false;
for (let index = 0; index < args.length; ++index) {
  const arg = args[index];
  if (arg === '--version' || arg.startsWith('--version=')) {
    if (version !== undefined) {
      invalid = true;
    }
    version = arg === '--version' ? args[++index] : arg.slice('--version='.length);
  } else if (arg === '--with-milkdrop' && !withMilkdrop) {
    withMilkdrop = true;
  } else if (arg === '--clean' && !clean) {
    clean = true;
  } else if (arg === '--skip-archive' && !skipArchive) {
    skipArchive = true;
  } else {
    invalid = true;
  }
}
if (invalid || !version || !/^[a-zA-Z0-9][a-zA-Z0-9._+-]*$/.test(version)) {
  console.error(usage);
  process.exit(1);
}

const root = resolve(__dirname, '..');
const build = join(root, 'build', 'windows-x64');
const source = join(root, 'bin64', 'Release');
const dist = join(root, 'dist', version);

function run(command, args, cwd = root) {
  const result = spawnSync(command, args, { cwd, stdio: 'inherit' });
  if (result.error) {
    throw result.error;
  }
  if (result.status !== 0) {
    throw new Error(`${command} failed with status ${result.status}`);
  }
}

function enabled(cache, name) {
  const match = cache.match(new RegExp(`^${name}:BOOL=(.*)$`, 'm'));
  const value = match && match[1].trim().toUpperCase();
  if (['1', 'ON', 'YES', 'TRUE', 'Y'].includes(value)) {
    return true;
  }
  if (['0', 'OFF', 'NO', 'FALSE', 'N', 'IGNORE', 'NOTFOUND'].includes(value) ||
      (value && value.endsWith('-NOTFOUND'))) {
    return false;
  }
  throw new Error(`Missing or invalid ${name} in ${join(build, 'CMakeCache.txt')}`);
}

function copyFiles(from, to, extension, include = () => true) {
  mkdirSync(to, { recursive: true });
  for (const name of readdirSync(from)) {
    if (name.toLowerCase().endsWith(extension) && include(name.toLowerCase())) {
      copyFileSync(join(from, name), join(to, name));
    }
  }
}

function zip(stage, name) {
  const archive = join(dist, `${name}.zip`);
  /* Recreating the ZIP prevents deleted files from surviving a repeat release. */
  rmSync(archive, { force: true });
  run('7z', ['a', '-tzip', archive, '.', '-mx=9'], stage);
}

try {
  const milkdropSolution = join(root, '..', 'milkdrop2-musikcube', 'milkdrop2-musikcube.sln');
  if (withMilkdrop && !existsSync(milkdropSolution)) {
    throw new Error(`MilkDrop checkout not found: ${milkdropSolution}`);
  }
  run(process.execPath, [join(root, 'script', 'generate-windows-solution.js')]);
  if (clean) {
    run('cmd.exe', ['/d', '/c', join(root, 'script', 'clean-win.bat')]);
  }
  run('cmake', ['--build', '--preset', 'windows-release', '--parallel']);
  if (withMilkdrop) {
    run('MSBuild.exe', [milkdropSolution, ...(clean ? ['/t:Rebuild'] : []), '/restore', '/p:RestorePackagesConfig=true', '/m', '/nologo',
      '/verbosity:minimal', '/p:Configuration=Release', '/p:Platform=x64', '/p:PlatformToolset=v145']);
  }
  if (skipArchive) {
    console.log('Build complete; skipping archives.');
    process.exit(0);
  }
  const cache = readFileSync(join(build, 'CMakeCache.txt'), 'utf8');
  const systemHttp = enabled(cache, 'MUSIKCORE_USE_WINDOWS_HTTP_CLIENT');
  const systemTls = enabled(cache, 'MUSIKCORE_USE_ASIO_WINTLS');

  if (withMilkdrop) {
    for (const name of MILKDROP_REQUIRED) {
      if (!existsSync(join(source, name))) {
        throw new Error(`Missing MilkDrop file: ${name}. Build with --with-milkdrop first.`);
      }
    }
  }

  const appName = `musikcube_${version}_win64`;
  const app = join(dist, appName);
  rmSync(app, { recursive: true, force: true });
  mkdirSync(app, { recursive: true });
  for (const name of APP_EXECUTABLES) {
    copyFileSync(join(source, name), join(app, name));
  }
  copyFiles(source, app, '.dll', name =>
    !(systemHttp && CURL_DLLS.includes(name)) &&
    !(systemTls && OPENSSL_DLLS.includes(name)) &&
    !(!withMilkdrop && MILKDROP_DLLS.some(dll => dll.toLowerCase() === name)));
  copyFiles(join(source, 'plugins'), join(app, 'plugins'), '.dll', name =>
    !SAMPLE_PLUGINS.includes(name) && (withMilkdrop || name !== MILKDROP_PLUGIN));
  cpSync(join(source, 'plugins', PROJECTM_RESOURCES), join(app, 'plugins', PROJECTM_RESOURCES), { recursive: true });
  if (withMilkdrop) {
    cpSync(join(source, 'plugins', MILKDROP_RESOURCES), join(app, 'plugins', MILKDROP_RESOURCES), { recursive: true });
  }
  for (const [directory, extension] of Object.entries(ASSET_EXTENSIONS)) {
    copyFiles(join(source, directory), join(app, directory), extension);
  }
  zip(app, appName);

  const sdkName = `musikcore_sdk_${version}_win64`;
  const sdk = join(dist, sdkName);
  rmSync(sdk, { recursive: true, force: true });
  mkdirSync(sdk, { recursive: true });
  run('cmake', ['--install', build, '--config', 'Release', '--component', 'musikcoreSDK', '--prefix', sdk]);
  zip(sdk, sdkName);
}
catch (error) {
  console.error(error.message);
  process.exit(1);
}
