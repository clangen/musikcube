/* Shared measurement helpers for TUI scenarios. Repeat and alternate variants,
   summarize latency samples, and convert Linux CPU-accounting ticks to time.
   Scenario code supplies the interactions and correctness assertions. */

const { execFile, execFileSync } = require('node:child_process');
const { promisify } = require('node:util');

/* Collect repeated scenario measurements, alternating variant order to reduce warm-cache bias. */
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

/* Validate available measurements and print scaled median and nearest-rank p95 for each variant. */
function printMetrics(results, metrics) {
  for (const { variant, samples } of results) {
    console.log(variant.name);
    for (const [label, key, unit, scale = 1] of metrics) {
      const values = samples.map(sample => sample[key]);
      if (values.every(value => value === undefined)) continue;
      if (!values.every(Number.isFinite)) throw new Error(`Missing or invalid sample for ${label}`);

      /* Missing a platform-only metric is valid; partial/invalid samples above
         are not, since silently dropping them would bias the comparison. */
      const sorted = values.map(value => value * scale).sort((a, b) => a - b);
      const middle = Math.floor(sorted.length / 2);
      const median = sorted.length % 2 ? sorted[middle] : (sorted[middle - 1] + sorted[middle]) / 2;
      const p95 = sorted[Math.ceil(sorted.length * 0.95) - 1];
      console.log(`${label}: median ${median.toFixed(1)} ${unit}, p95 ${p95.toFixed(1)} ${unit}`);
    }
  }
}

/* Read Linux's process-accounting tick rate; leave CPU metrics unavailable on other platforms. */
async function cpuMillisecondsPerTick() {
  if (process.platform !== 'linux') return undefined;
  const { stdout } = await promisify(execFile)('getconf', ['CLK_TCK']);
  const rate = Number(stdout.trim());
  if (!Number.isSafeInteger(rate) || rate <= 0) {
    throw new Error('Could not determine the process CPU clock tick rate');
  }

  return 1000 / rate;
}

/* Query cumulative CPU time and memory for the app itself using Windows process counters. */
function windowsProcessStats(pid) {
  if (!Number.isSafeInteger(pid) || pid <= 0) throw new Error('Invalid Windows app process ID');
  const command = `$p = Get-Process -Id ${pid} -ErrorAction Stop; ` +
    '@{ cpuMs = $p.TotalProcessorTime.TotalMilliseconds; workingSetBytes = $p.WorkingSet64; ' +
    'peakRssKiB = $p.PeakWorkingSet64 / 1KB; privateBytes = $p.PrivateMemorySize64 } | ConvertTo-Json -Compress';
  return JSON.parse(execFileSync('powershell.exe', ['-NoProfile', '-NonInteractive', '-Command', command],
    { encoding: 'utf8', windowsHide: true, timeout: 5000 }));
}

module.exports = { sampleVariants, printMetrics, cpuMillisecondsPerTick, windowsProcessStats };
