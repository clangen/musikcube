/* Compare Windows Release builds with the same seeded local catalog and null output. */
const fs = require('node:fs');
const { dirname, join, relative, resolve } = require('node:path');
const { performance } = require('node:perf_hooks');
const { setTimeout: delay } = require('node:timers/promises');
const { Terminal } = require('../ui/support/terminal');
const { sampleVariants, printMetrics } = require('../ui/support/benchmark');
const { withRuntime } = require('../ui/support/artifacts');
const { FIRST_TRACK, FILTER, FILTERED_FIRST } = require('../ui/support/catalog');
const { stageTestRuntime } = require('./test-runtime');

async function main() {
  const [host, database, baseline, current, output, runCount = '3'] = process.argv.slice(2);
  const runs = Number(runCount);
  if (process.platform !== 'win32' || !output || !Number.isSafeInteger(runs) || runs < 1) {
    throw new Error('Usage: node benchmark-windows.js <ConPTY host> <seeded musik.db> <baseline bin> <current bin> <output dir> [runs]');
  }
  const windowsHost = resolve(host);
  const catalog = resolve(database);
  const variants = [
    { name: 'baseline', path: resolve(baseline) },
    { name: 'current', path: resolve(current) },
  ];
  const root = resolve(output);
  fs.mkdirSync(root, { recursive: true });

  const results = await sampleVariants(variants, runs, async variant => {
    const fixture = fs.mkdtempSync(join(root, variant.name + '-'));
    return withRuntime(fixture, async () => {
      const runtime = dirname(stageTestRuntime(join(variant.path, 'musikcube-cmd.exe'), fixture));
      for (const directory of ['locales', 'themes']) {
        fs.cpSync(join(variant.path, directory), join(runtime, directory), { recursive: true });
      }
      fs.copyFileSync(join(variant.path, 'plugins', 'nullout.dll'), join(runtime, 'plugins', 'nullout.dll'));

      /* Copy the committed database, keeping its absolute references to the same generated media. */
      const profile = join(fixture, 'profile');
      const catalogRoot = dirname(dirname(dirname(catalog)));
      const target = join(profile, relative(catalogRoot, catalog));
      fs.mkdirSync(dirname(target), { recursive: true });
      fs.copyFileSync(catalog, target);
      fs.writeFileSync(join(profile, 'musikcube', 'settings.json'), JSON.stringify({
        FirstRunSettingsDisplayed: true, SyncOnStartup: false, AutoUpdateCheck: false,
      }));

      const started = performance.now();
      const terminal = new Terminal(runtime, { ...process.env, APPDATA: profile, USERPROFILE: profile },
        26, 140, { windowsHost });
      try {
        await terminal.expect('^D  quit');
        const startupMs = performance.now() - started;
        const startup = terminal.processStats();
        const searchStarted = performance.now();
        let searchViewMs;
        const observeSearchView = () => {
          if (searchViewMs === undefined && terminal.screen.text().includes('tracks (sorted by album)')) {
            searchViewMs = performance.now() - searchStarted;
          }
        };
        terminal.events.on('update', observeSearchView);
        terminal.send('t');
        try {
          await terminal.expect(FIRST_TRACK);
        } finally {
          terminal.events.off('update', observeSearchView);
        }
        const searchMs = performance.now() - searchStarted;
        const searched = terminal.processStats();
        const filterStarted = performance.now();
        terminal.send(FILTER);
        await terminal.expect(FILTERED_FIRST, { absent: FIRST_TRACK });
        const filterMs = performance.now() - filterStarted;

        await delay(400);
        const idle = terminal.processStats();
        const idleStarted = performance.now();
        await delay(2000);
        const ready = terminal.processStats();
        const idleSeconds = (performance.now() - idleStarted) / 1000;

        terminal.send('\r');
        await terminal.expect('playing ' + FILTERED_FIRST);
        const playback = terminal.processStats();
        const playbackStarted = performance.now();
        await terminal.waitFor(() => terminal.screen.text().includes('playing ') &&
          !terminal.screen.text().includes('playing ' + FILTERED_FIRST), 'playback to advance to another track');
        await delay(Math.max(0, 4000 - (performance.now() - playbackStarted)));
        const finished = terminal.processStats();
        const playbackSeconds = (performance.now() - playbackStarted) / 1000;
        terminal.send('\x04');
        await terminal.expectExit();
        return {
          startupMs, searchViewMs, searchMs, filterMs,
          searchRowsMs: searchViewMs === undefined ? undefined : searchMs - searchViewMs,
          startupPrivateBytes: startup.privateBytes,
          searchPrivateBytes: searched.privateBytes,
          filterPrivateBytes: ready.privateBytes,
          searchCpuMs: searched.cpuMs - startup.cpuMs,
          idleCpuMs: (ready.cpuMs - idle.cpuMs) / idleSeconds,
          playbackCpuMs: (finished.cpuMs - playback.cpuMs) / playbackSeconds,
          privateBytes: finished.privateBytes,
          workingSetBytes: finished.workingSetBytes,
          peakRssKiB: finished.peakRssKiB,
        };
      } catch (error) {
        terminal.saveArtifacts(fixture);
        throw error;
      } finally {
        await terminal.close();
      }
    });
  });
  fs.writeFileSync(join(root, 'results.json'), JSON.stringify(results, null, 2) + '\n');
  printMetrics(results, [
    ['Startup', 'startupMs', 'ms'], ['Open search view', 'searchViewMs', 'ms'],
    ['Open track search', 'searchMs', 'ms'], ['View to first track', 'searchRowsMs', 'ms'],
    ['Filter catalog', 'filterMs', 'ms'], ['Search CPU', 'searchCpuMs', 'ms'],
    ['Idle CPU', 'idleCpuMs', 'ms/s'], ['Playback CPU', 'playbackCpuMs', 'ms/s'],
    ['Startup private memory', 'startupPrivateBytes', 'MiB', 1 / 1048576],
    ['Search private memory', 'searchPrivateBytes', 'MiB', 1 / 1048576],
    ['Filtered private memory', 'filterPrivateBytes', 'MiB', 1 / 1048576],
    ['Playback private memory', 'privateBytes', 'MiB', 1 / 1048576],
    ['Working set', 'workingSetBytes', 'MiB', 1 / 1048576], ['Peak working set', 'peakRssKiB', 'MiB', 1 / 1024],
  ]);
}

main().catch(error => { console.error(error.message); process.exitCode = 1; });
