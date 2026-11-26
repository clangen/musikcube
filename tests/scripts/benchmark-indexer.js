/* Measure incremental production indexing with many distinct metadata values. */
const { spawnSync } = require('node:child_process');
const { cpus, platform, arch, release } = require('node:os');
const { copyFileSync, existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } = require('node:fs');
const { basename, dirname, join, resolve } = require('node:path');
const { parseArgs } = require('node:util');
const { stageTestRuntime } = require('./test-runtime');

const { values, positionals } = parseArgs({
  allowPositionals: true,
  options: {
    baseline: { type: 'string' },
    batches: { type: 'string', default: '10' },
    workload: { type: 'string', default: 'metadata' },
    out: { type: 'string' },
  },
});
const [buildArgument] = positionals;
const batches = Number(values.batches);
if (positionals.length !== 1 || !Number.isSafeInteger(batches) || batches < 1 || batches > 100 ||
    !['metadata', 'gme', 'openmpt'].includes(values.workload)) {
  throw new Error('Usage: node tests/scripts/benchmark-indexer.js <build> [--workload metadata|gme|openmpt] [--batches N] [--baseline FILE] [--out FILE]');
}

const source = resolve(__dirname, '../..');
const build = resolve(buildArgument);
const workloads = {
  metadata: { id: 'incremental-wav-distinct-comments-v1', files: 5000, tracks: 1, extension: 'wav',
    description: '5000 new WAVs per scan; unique comments, 1000 artists, 2000 albums; includes a 3 second idle wait' },
  gme: { id: 'incremental-nsf-16-tracks-v1', files: 1000, tracks: 16, extension: 'nsf',
    description: '1000 new NSF files per scan, 16 tracks each; includes a 3 second idle wait' },
  openmpt: { id: 'incremental-mod-two-subsongs-v1', files: 2500, tracks: 2, extension: 'mod',
    description: '2500 new MOD files per scan, two subsongs each; includes a 3 second idle wait' },
};
const selected = workloads[values.workload];
const workload = selected.id;
const baseline = values.baseline ? JSON.parse(readFileSync(resolve(values.baseline), 'utf8')) : null;
if (baseline && baseline.workload !== workload) {
  throw new Error('Baseline uses a different indexing workload');
}

function run(command, args, options = {}) {
  const result = spawnSync(command, args, { cwd: source, encoding: 'utf8', ...options });
  if (result.error || result.status !== 0) {
    throw result.error || new Error(`${command} failed (${result.signal || result.status}):\n${result.stdout}\n${result.stderr}`);
  }
  return result.stdout;
}

function chunk(id, data) {
  const header = Buffer.alloc(8);
  header.write(id);
  header.writeUInt32LE(data.length, 4);
  return Buffer.concat([header, data, Buffer.alloc(data.length % 2)]);
}

function wave(index) {
  const format = Buffer.alloc(16);
  format.writeUInt16LE(1, 0);
  format.writeUInt16LE(1, 2);
  format.writeUInt32LE(8000, 4);
  format.writeUInt32LE(16000, 8);
  format.writeUInt16LE(2, 12);
  format.writeUInt16LE(16, 14);
  const fields = [
    ['INAM', `Track ${index}`], ['IART', `Artist ${index % 1000}`],
    ['IPRD', `Album ${index % 2000}`], ['ICMT', `Distinct comment ${index}`],
  ].map(([key, value]) => chunk(key, Buffer.from(value + '\0')));
  return chunk('RIFF', Buffer.concat([
    Buffer.from('WAVE'), chunk('fmt ', format), chunk('LIST', Buffer.concat([Buffer.from('INFO'), ...fields])),
    chunk('data', Buffer.alloc(160)),
  ]));
}

function nsf(index) {
  const bytes = Buffer.alloc(129);
  Buffer.from('NESM\x1a', 'binary').copy(bytes);
  bytes[5] = 1;
  bytes[6] = 16;
  bytes[7] = 1;
  for (const high of [9, 11, 13]) bytes[high] = 0x80;
  bytes.write(`Game ${index}`, 14, 32);
  bytes[110] = 0x1a;
  bytes[111] = 0x41;
  bytes[128] = 0x60;
  return bytes;
}

function mod(index) {
  const bytes = Buffer.alloc(1084 + 2 * 64 * 4 * 4);
  bytes.write(`Module ${index}`, 0, 20);
  bytes[950] = 2;
  bytes[953] = 1;
  bytes[1086] = 0x0b;
  bytes.write('M.K.', 1080);
  return bytes;
}

run('cmake', ['-S', source, '-B', build]);
run('cmake', ['--build', build, '--config', 'Release', '--target', 'musikcube_indexer_benchmark']);
const executable = readFileSync(join(build, 'indexer-benchmark-Release.txt'), 'utf8').trim();
const artifacts = join(build, 'indexer-benchmark');
mkdirSync(artifacts, { recursive: true });
const fixture = mkdtempSync(join(artifacts, 'run-'));
const media = join(fixture, 'media');
const profile = join(fixture, 'profile');
mkdirSync(media);
mkdirSync(profile);
const staged = stageTestRuntime(executable, fixture, false);
const plugins = join(fixture, 'runtime', 'plugins');
const sourcePlugins = join(dirname(executable), 'plugins');
let tagReader = false;
let gme = false;
let openmpt = false;
for (const entry of readdirSync(sourcePlugins)) {
  const tag = /^(lib)?(taglibreader|taglib_plugin)\.(dll|dylib|so)$/i.test(entry);
  const game = /^(lib)?gmedecoder\.(dll|dylib|so)$/i.test(entry);
  const module = /^(lib)?openmptdecoder\.(dll|dylib|so)$/i.test(entry);
  if ((values.workload === 'metadata' && (tag || game)) ||
      (values.workload === 'gme' && game) || (values.workload === 'openmpt' && module)) {
    copyFileSync(join(sourcePlugins, entry), join(plugins, basename(entry)));
    tagReader ||= tag;
    gme ||= game;
    openmpt ||= module;
  }
}
if ((values.workload === 'metadata' && (!tagReader || !gme)) ||
    (values.workload === 'gme' && !gme) || (values.workload === 'openmpt' && !openmpt)) {
  throw new Error(`Missing providers required for the ${values.workload} workload`);
}

const samples = [];
for (let batch = 0; batch < batches; ++batch) {
  const directory = join(media, `batch-${batch}`);
  mkdirSync(directory);
  for (let offset = 0; offset < selected.files; ++offset) {
    const index = batch * selected.files + offset;
    const bytes = values.workload === 'gme' ? nsf(index) : values.workload === 'openmpt' ? mod(index) : wave(index);
    writeFileSync(join(directory, `track-${String(index).padStart(6, '0')}.${selected.extension}`), bytes);
  }
  const expected = (batch + 1) * selected.files * selected.tracks;
  console.log(`Indexing ${expected} tracks...`);
  const started = performance.now();
  run(staged, [media, plugins], { env: { ...process.env, APPDATA: profile, USERPROFILE: profile, XDG_CONFIG_HOME: profile },
    timeout: 300000 });
  const seconds = (performance.now() - started) / 1000;
  const database = join(profile, 'musikcube', '1', 'musik.db');
  if (!existsSync(database)) throw new Error(`Missing indexed catalog: ${database}`);
  const condition = values.workload === 'metadata' ? 'source_id=0' : 'source_id<>0';
  const count = Number(run('sqlite3', ['-readonly', database, `SELECT count(*) FROM tracks WHERE ${condition}`]).trim());
  if (count !== expected) throw new Error(`Expected ${expected} tracks; found ${count}`);
  const previous = baseline?.samples.find(sample => sample.tracks === count);
  const sample = { tracks: count, seconds: Number(seconds.toFixed(3)) };
  samples.push(sample);
  console.log(previous ? `${sample.seconds}s (baseline ${previous.seconds}s)` : `${sample.seconds}s`);
}

const result = {
  workload,
  description: selected.description,
  platform: platform(), architecture: arch(), osRelease: release(), cpu: cpus()[0]?.model,
  configuration: 'Release',
  sourceCommit: run('git', ['rev-parse', 'HEAD']).trim(),
  sourceModified: !!run('git', ['status', '--porcelain']).trim(),
  samples,
};
const destination = resolve(values.out || join(artifacts, 'results.json'));
mkdirSync(dirname(destination), { recursive: true });
writeFileSync(destination, `${JSON.stringify(result, null, 2)}\n`);
console.log(`Results: ${destination}\nFixtures: ${fixture}`);
