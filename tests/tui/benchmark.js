const { execFile } = require('node:child_process');
const { promisify } = require('node:util');

async function sampleVariants(variants, runs, run) {
  const results = variants.map(variant => ({ variant, samples: [] }));

  /* Alternate launch order to reduce the effect of warmed OS caches. */
  for (let i = 0; i < runs; ++i) {
    const order = i % 2 ? [...results].reverse() : results;
    for (const { variant, samples } of order) {
      samples.push(await run(variant));
    }
  }

  return results;
}

function printMetrics(results, metrics) {
  for (const { variant, samples } of results) {
    console.log(variant.name);
    for (const [label, key, unit, scale = 1] of metrics) {
      const values = samples.map(sample => sample[key]);
      if (values.every(value => value === undefined)) continue;
      if (!values.every(Number.isFinite)) throw new Error(`Missing or invalid sample for ${label}`);

      const sorted = values.map(value => value * scale).sort((a, b) => a - b);
      const middle = Math.floor(sorted.length / 2);
      const median = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
      const p95 = sorted[Math.ceil(sorted.length * 0.95) - 1];
      console.log(`${label}: median ${median.toFixed(1)} ${unit}, p95 ${p95.toFixed(1)} ${unit}`);
    }
  }
}

async function cpuMillisecondsPerTick() {
  if (process.platform !== 'linux') return undefined;
  const { stdout } = await promisify(execFile)('getconf', ['CLK_TCK']);
  const rate = Number(stdout.trim());
  if (!Number.isSafeInteger(rate) || rate <= 0) {
    throw new Error('Could not determine the process CPU clock tick rate');
  }
  return 1000 / rate;
}

module.exports = { sampleVariants, printMetrics, cpuMillisecondsPerTick };
