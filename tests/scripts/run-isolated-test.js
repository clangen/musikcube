/* Launch a native C/C++ test with private configuration and catalog directories.
   Production fixtures can also stage their executable and required plugins.
   The native executable performs the assertions; failed runs retain their files. */
const { spawnSync } = require('child_process');
const { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } = require('fs');
const { basename, dirname, extname, join, resolve } = require('path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable, ...args] = process.argv.slice(2);
if (!parent || !executable) throw new Error('Expected fixture parent and executable.');

/* Own only this run's root; a supplied config root belongs to the caller. */
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'run-'));
let command = executable;
let config = root;

/* fatal lifecycle tests exercise real aborts, retaining the same private
runtime setup instead of substituting a process-wide termination callback. */
const expectAbort = args[0] === '--expect-abort';
if (expectAbort) args.shift();

/* Production fixtures load a staged executable/plugins; standalone ABI tests
   can run directly with their existing shared-library search paths. */
if (args[0] === '--stage' || args[0] === '--stage-minimal') {
  const includeDefaultPlugins = args.shift() === '--stage';
  command = stageTestRuntime(resolve(executable), root, includeDefaultPlugins);
}

if (args[0] === '--locale') {
  args.shift();
  const locale = args.shift();
  const directory = join(dirname(command), 'locales');
  mkdirSync(directory, { recursive: true });
  copyFileSync(locale, join(directory, basename(locale)));
}

if (args[0] === '--headless-output') {
  args.shift();
  const plugins = join(dirname(command), 'plugins');
  const output = args.shift();
  const extension = extname(output);
  copyFileSync(output, join(plugins, basename(output)));
  /* Exercise dlopen failure alongside the working headless output. */
  writeFileSync(join(plugins, `libunavailable-output${extension}`), 'Invalid shared library\n');
}

while (args[0] === '--plugin') {
  args.shift();
  const plugin = args.shift();
  copyFileSync(plugin, join(dirname(command), 'plugins', basename(plugin)));
}

if (args[0] === '--config-root') {
  args.shift();
  config = resolve(args.shift());
}

/* Expand named catalog arguments into private directories, allowing switch
   tests to reuse the same command on every supported platform. */
const fixtures = ['origin', 'destination'].map(name => {
  const directory = join(root, name);
  mkdirSync(directory);
  return directory;
});

const expanded = args.map(value => value === '@origin' ? fixtures[0]
  : value === '@destination' ? fixtures[1] : value);

/* Bound the child lifetime and redirect both Unix and Windows setting roots. */
const result = spawnSync(command, expanded, {
  stdio: 'inherit',
  env: {
    ...process.env,
    XDG_CONFIG_HOME: config,
    APPDATA: config,
    ...(expectAbort ? { MUSIKCORE_TEST_EXPECT_ABORT: '1' } : {}),
  },
  timeout: 90000,
});

/* Windows reports either CRT abort or fast-fail termination; Unix reports
SIGABRT. callers also check the expected fatal diagnostic. */
const passed = !result.error && (expectAbort
  ? result.signal === 'SIGABRT' || process.platform === 'win32' && (result.status === 3 || (result.status >>> 0) === 0xc0000409)
  : result.status === 0);

/* successful runs leave no state; failures retain their isolated evidence. */
if (passed) rmSync(root, { recursive: true });
else console.error(`Test failed; isolated fixture retained at ${root}`);
if (result.error) console.error(result.error.message);

process.exitCode = passed ? 0 : result.status || 1;
