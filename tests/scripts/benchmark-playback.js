/* Record one isolated playback process per sample so peak RSS is comparable. */
const { execFileSync } = require('node:child_process');
const { mkdirSync, readFileSync, writeFileSync } = require('node:fs');
const { basename, dirname, join, resolve } = require('node:path');
const { parseArgs } = require('node:util');

const { positionals, values } = parseArgs({
  allowPositionals: true,
  options: {
    runs: { type: 'string', default: '3' },
    baseline: { type: 'string' },
    'compare-binary': { type: 'string' },
    'compare-plugin': { type: 'string' },
    out: { type: 'string' },
    'max-cpu-increase': { type: 'string' },
    'max-rss-increase': { type: 'string' },
  },
});
const [build, executable, plugin] = positionals;
const runs = Number(values.runs);
const compareBinary = values['compare-binary'];
const comparePlugin = values['compare-plugin'];
if (positionals.length !== 3 || !Number.isSafeInteger(runs) || runs <= 0 ||
    Boolean(compareBinary) !== Boolean(comparePlugin) || (values.baseline && compareBinary)) {
  throw new Error('Usage: node benchmark-playback.js <build> <test-binary> <capture-plugin> ' +
    '[--runs N] [--baseline FILE | --compare-binary FILE --compare-plugin FILE] ' +
    '[--out FILE] [--max-cpu-increase PCT] [--max-rss-increase PCT]');
}

const workload = 'gapless-wav-48khz-stereo-v1';
const destination = resolve(values.out ?? join(build, 'playback-benchmark', 'results.json'));
const baseline = values.baseline ? JSON.parse(readFileSync(values.baseline, 'utf8')) : null;
if (baseline && baseline.workload !== workload) {
  throw new Error('Baseline was recorded with a different playback workload');
}

const marker = 'playback-benchmark ';
function measure(binary, capture) {
  const log = execFileSync(process.execPath, [
    join(__dirname, 'run-isolated-test.js'), join(build, 'playback-benchmark', 'isolated'),
    resolve(binary), '--stage', '--plugin', resolve(capture),
    'benchmark', basename(capture), '@origin',
  ], { encoding: 'utf8', timeout: 20000 });
  const line = log.split(/\r?\n/).find(entry => entry.startsWith(marker));
  if (!line) throw new Error('Playback benchmark did not report a sample');
  return JSON.parse(line.slice(marker.length));
}

const samples = [];
const comparison = [];
for (let i = 0; i < runs; ++i) {
  /* Reverse each pair to balance any first-run or cache effects. */
  if (compareBinary && i % 2 === 0) {
    comparison.push(measure(compareBinary, comparePlugin));
  }
  samples.push(measure(executable, plugin));
  if (compareBinary && i % 2 !== 0) {
    comparison.push(measure(compareBinary, comparePlugin));
  }
}

const metrics = [
  ['CPU', 'cpuMs', 'ms', 1, values['max-cpu-increase']],
  ['Peak RSS', 'peakRssBytes', 'MiB', 1024 ** 2, values['max-rss-increase']],
  ['Elapsed', 'wallMs', 'ms', 1],
];
const summary = {};
function summarize(values, name) {
  const sorted = [...values].sort((a, b) => a - b);
  if (!sorted.every(Number.isFinite)) throw new Error(`Invalid ${name} sample`);
  const middle = Math.floor(sorted.length / 2);
  const median = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
  return { median, p95: sorted[Math.ceil(sorted.length * 0.95) - 1], min: sorted[0], max: sorted.at(-1) };
}

const paired = {};
for (const [name, key, unit, scale, budget] of metrics) {
  const { median, p95 } = summary[key] = summarize(samples.map(sample => sample[key]), name);
  const previous = baseline?.summary?.[key]?.median;
  let increase = previous > 0 ? 100 * (median / previous - 1) : undefined;
  let change = increase === undefined ? '' : ` (${increase.toFixed(1)}% vs baseline)`;
  if (compareBinary) {
    const differences = samples.map((sample, i) => 100 * (sample[key] / comparison[i][key] - 1));
    const delta = paired[key] = summarize(differences, name);
    increase = delta.median;
    change = ` (paired median ${increase.toFixed(1)}%, range ${delta.min.toFixed(1)}% to ${delta.max.toFixed(1)}%)`;
  }
  console.log(`${name} median ${(median / scale).toFixed(2)} ${unit}, p95 ${(p95 / scale).toFixed(2)} ${unit}${change}`);
  if (budget !== undefined) {
    const allowed = Number(budget);
    if (budget.trim() === '' || !Number.isFinite(allowed) || allowed < 0 || increase === undefined) {
      throw new Error(`A ${name} budget requires a baseline and a nonnegative percentage`);
    }
    if (increase > allowed) {
      console.error(`${name} increased ${increase.toFixed(1)}%, exceeding the ${allowed}% budget`);
      process.exitCode = 1;
    }
  }
}

mkdirSync(dirname(destination), { recursive: true });
writeFileSync(destination, JSON.stringify({ workload, samples, summary, comparison, paired }, null, 2) + '\n');
console.log(`Samples saved to ${destination}`);
