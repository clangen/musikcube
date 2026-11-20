/* Run one fixture with fresh state; retain failures for diagnosis. */
const { spawnSync } = require('child_process');
const { mkdirSync, mkdtempSync, rmSync } = require('fs');
const { join, resolve } = require('path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable, ...args] = process.argv.slice(2);
if (!parent || !executable) throw new Error('Expected fixture parent and executable.');
mkdirSync(parent, { recursive: true });
const root = mkdtempSync(join(resolve(parent), 'run-'));
let command = executable;
let config = root;
if (args[0] === '--stage') {
  args.shift();
  command = stageTestRuntime(resolve(executable), root);
}
if (args[0] === '--config-root') {
  args.shift();
  config = resolve(args.shift());
}
const fixtures = ['origin', 'destination'].map(name => {
  const directory = join(root, name);
  mkdirSync(directory);
  return directory;
});
const expanded = args.map(value => value === '@origin' ? fixtures[0]
  : value === '@destination' ? fixtures[1] : value);
const result = spawnSync(command, expanded, {
  stdio: 'inherit',
  env: { ...process.env, XDG_CONFIG_HOME: config, APPDATA: config },
  timeout: 90000,
});
if (result.status === 0) rmSync(root, { recursive: true });
else console.error(`Test failed; isolated fixture retained at ${root}`);
if (result.error) console.error(result.error.message);
process.exitCode = result.status ?? 1;
