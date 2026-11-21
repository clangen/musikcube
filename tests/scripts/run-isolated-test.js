/* Launch a native C/C++ test with private configuration and catalog directories.
   Production fixtures can also stage their executable and required plugins.
   The native executable performs the assertions; failed runs retain their files. */
const { spawnSync } = require('child_process');
const { copyFileSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } = require('fs');
const { basename, dirname, join, resolve } = require('path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable, ...args] = process.argv.slice(2);
if (!parent || !executable) throw new Error('Expected fixture parent and executable.');

/* Own only this run's root; a supplied config root belongs to the caller. */
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'run-'));
let command = executable;
let config = root;

/* Production fixtures load a staged executable/plugins; standalone ABI tests
   can run directly with their existing shared-library search paths. */
if (args[0] === '--stage') {
  args.shift();
  command = stageTestRuntime(resolve(executable), root);
}

if (args[0] === '--headless-output') {
  args.shift();
  const plugins = join(dirname(command), 'plugins');
  copyFileSync(args.shift(), join(plugins, 'libnullout.so'));
  /* Exercise dlopen failure alongside the working headless output. */
  writeFileSync(join(plugins, 'libunavailable-output.so'), 'Invalid shared library\n');
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

/* Bound the child lifetime and redirect both Unix and Windows preference roots. */
const result = spawnSync(command, expanded, {
  stdio: 'inherit',
  env: { ...process.env, XDG_CONFIG_HOME: config, APPDATA: config },
  timeout: 90000,
});

/* Successful runs leave no state; failures retain their isolated evidence. */
if (result.status === 0) rmSync(root, { recursive: true });
else console.error(`Test failed; isolated fixture retained at ${root}`);
if (result.error) console.error(result.error.message);

process.exitCode = result.status ?? 1;
