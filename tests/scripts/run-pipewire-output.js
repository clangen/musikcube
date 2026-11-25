/* run the native output against an isolated PipeWire/PulseAudio null sink. */
const { spawn, spawnSync } = require('node:child_process');
const { chmodSync, closeSync, copyFileSync, mkdirSync, mkdtempSync, openSync, rmSync } = require('node:fs');
const { basename, join, resolve } = require('node:path');
const { stageTestRuntime } = require('./test-runtime');

const [parent, executable, plugin, driver, preload] = process.argv.slice(2);
if (!parent || !executable || !plugin) throw new Error('Expected fixture root, test executable, and plugin.');
const fixtureParent = join(resolve(parent), 'tests', 'bin', 'isolated');
mkdirSync(fixtureParent, { recursive: true });
const root = mkdtempSync(join(fixtureParent, 'pipewire-'));
chmodSync(root, 0o700);
const env = { ...process.env, XDG_RUNTIME_DIR: root, PIPEWIRE_RUNTIME_DIR: root,
  XDG_CONFIG_HOME: join(root, 'config'), XDG_CACHE_HOME: join(root, 'cache'),
  XDG_STATE_HOME: join(root, 'state'), PIPEWIRE_REMOTE: 'pipewire-0',
  PULSE_SERVER: `unix:${join(root, 'pulse', 'native')}` };
const logs = openSync(join(root, 'services.log'), 'w');
const processes = [];
let passed = false;
const delay = ms => new Promise(resolveDelay => setTimeout(resolveDelay, ms));

function start(command) {
  const child = spawn(command, [], { env, stdio: ['ignore', logs, logs] });
  child.on('error', () => {});
  processes.push(child);
}

function run(command, args = []) {
  return spawnSync(command, args, { env, encoding: 'utf8', timeout: 2000 });
}

async function main() {
  start('pipewire');
  /* startup has no notification shared by all tools; bound readiness probes. */
  let ready = false;
  for (let i = 0; i < 100; ++i) {
    if (run('pw-cli', ['info', '0']).status === 0) {
      ready = true;
      break;
    }
    await delay(100);
  }
  if (!ready) throw new Error('PipeWire did not start');
  start('wireplumber');
  start('pipewire-pulse');
  ready = false;
  for (let i = 0; i < 100; ++i) {
    if (run('pactl', ['info']).status === 0) {
      ready = true;
      break;
    }
    await delay(100);
  }
  if (!ready) throw new Error('PipeWire PulseAudio service did not start');
  const sink = run('pactl', ['load-module', 'module-null-sink', 'sink_name=musikcube_plugin_test']);
  if (sink.status !== 0) throw new Error(`Cannot create null sink: ${sink.stderr}`);
  /* Some PulseAudio compatibility servers accept the module request without
     publishing a module. Create a persistent native sink in that case. */
  if (!sink.stdout.trim()) {
    const created = run('pw-cli', ['create-node', 'adapter',
      '{ factory.name = support.null-audio-sink node.name = musikcube_plugin_test media.class = Audio/Sink object.linger = true audio.position = [ FL FR ] }']);
    if (created.status !== 0) throw new Error(`Cannot create PipeWire null sink: ${created.stderr}`);
  }
  let id;
  let nodes = [];
  for (let i = 0; i < 100; ++i) {
    const dump = run('pw-dump');
    if (dump.status === 0) {
      nodes = JSON.parse(dump.stdout).filter(item => item.type === 'PipeWire:Interface:Node');
      const node = nodes.find(item => item.info?.props?.['node.name'] === 'musikcube_plugin_test');
      if (node) {
        id = String(node.id);
        break;
      }
    }
    await delay(100);
  }
  if (!id) {
    throw new Error(`The null sink was not published: ${nodes.map(node => node.info?.props?.['node.name']).join(', ')}`);
  }
  let command = executable;
  let args = preload ? [plugin, "no-wait"] : [plugin];
  if (driver) {
    command = stageTestRuntime(resolve(executable), root);
    copyFileSync(plugin, join(root, 'runtime', 'plugins', basename(plugin)));
    args = [driver, driver === 'PulseAudio' ? 'musikcube_plugin_test' : ''];
  }
  const result = spawnSync(command, args, {
    env: { ...env, MUSIKCUBE_TEST_PIPEWIRE_DEVICE: id, ...(preload ? { LD_PRELOAD: preload } : {}) }, encoding: 'utf8', timeout: 15000,
  });
  if (result.status !== 0) throw new Error(`PipeWire output failed: ${result.stderr || result.signal || result.error}`);
  passed = true;
}

main().catch(error => {
  console.error(`${error.message}; service log: ${join(root, 'services.log')}`);
  process.exitCode = 1;
}).finally(async () => {
  const exited = processes.map(child => child.exitCode !== null || child.signalCode !== null
    ? Promise.resolve() : new Promise(resolveExit => child.once('exit', resolveExit)));
  for (const child of processes.reverse()) {
    if (child.exitCode === null && child.signalCode === null) child.kill('SIGTERM');
  }
  await Promise.race([Promise.all(exited), delay(500)]);
  for (const child of processes) {
    if (child.exitCode === null && child.signalCode === null) child.kill('SIGKILL');
  }
  await Promise.race([Promise.all(exited), delay(1000)]);
  closeSync(logs);
  if (passed) rmSync(root, { recursive: true, force: true });
});
