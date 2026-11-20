const { spawn } = require('node:child_process');
const fs = require('node:fs');
const { dirname, join, resolve } = require('node:path');
const { performance } = require('node:perf_hooks');
const { parseArgs } = require('node:util');
const { sampleVariants, printMetrics, cpuMillisecondsPerTick } = require('./benchmark');
const { Terminal } = require('./terminal');

const SOURCE = resolve(__dirname, '../..');
const TRACK_COUNT = 10000;
const FILTER = 'Artist 001 - Album 02';
const FIRST_ALBUM = 'Artist 001 - Album 01';
const FIRST_TRACK = `${FIRST_ALBUM} - Track 01`;
const FILTERED_FIRST = `${FILTER} - Track 01`;
const FILTERED_LAST = `${FILTER} - Track 10`;
const FIRST_WINDOW_MISS = `${FIRST_ALBUM} - Track 10`;
const DISTANT_TRACK = 'Artist 001 - Album 10 - Track 10';
const DOWN = '\x1bOB';
const UP = '\x1bOA';
const END = '\x1bOF';
const HOME = '\x1bOH';
const PAGE_DOWN = '\x1b[6~';
const DISTANT_PAGE = 12;

function stageRuntime(runtime, binaryDir) {
  /* Load only the plugins needed to index the fixture. */
  for (const name of fs.readdirSync(binaryDir)) {
    if (['lib', 'locales', 'share', 'themes'].includes(name) || name.startsWith('libmusikcore')) {
      fs.symlinkSync(fs.realpathSync(join(binaryDir, name)), join(runtime, name));
    }
  }

  for (const name of ['musikcube', 'musikcube_seed_library']) {
    fs.copyFileSync(join(binaryDir, name), join(runtime, name));
  }

  fs.mkdirSync(join(runtime, 'plugins'));
  for (const name of fs.readdirSync(join(binaryDir, 'plugins'))) {
    if (/^lib(?:taglibreader|ffmpegdecoder)\./.test(name)) {
      fs.symlinkSync(fs.realpathSync(join(binaryDir, 'plugins', name)), join(runtime, 'plugins', name));
    }
  }
}

async function seedLibrary(runtime, build, tracks) {
  const log = join(runtime, 'indexer.log');
  const output = fs.openSync(log, 'w');
  try {
    const child = spawn(process.execPath, [
      join(SOURCE, 'script/seed-test-library.js'),
      '--tracks', String(tracks),
      '--indexer', join(runtime, 'musikcube_seed_library'),
      '--fixture-dir', runtime, build,
    ], { timeout: 330000, stdio: ['ignore', output, output] });
    await new Promise((resolve, reject) => {
      child.once('error', reject);
      child.once('close', (code, signal) => {
        if (code === 0) resolve();
        else reject(new Error(`Seeder exited with ${signal || code}`));
      });
    });
  } catch (error) {
    throw new Error(`Indexing fixture failed\n${error.message}\n${fs.readFileSync(log, 'utf8')}`);
  } finally {
    fs.closeSync(output);
  }

  const summary = fs.readFileSync(log, 'utf8');
  const database = summary.trimEnd().split(/\r?\n/).pop();
  if (!fs.existsSync(database)) throw new Error(`Missing fixture database\n${summary}`);
  return database;
}

function stagePilot(runtime, binaryDir) {
  fs.mkdirSync(runtime);
  const library = process.platform === 'darwin' ? 'libmusikcore-abi0.dylib' : 'libmusikcore-abi0.so';
  fs.symlinkSync(fs.realpathSync(join(binaryDir, library)), join(runtime, library));
  fs.copyFileSync(join(binaryDir, 'musikcube_abi_search_pilot'), join(runtime, 'musikcube'));
}

async function checkTrackSearch(runtime, database, screenshots,
  { pilot = false, windows = false, cache } = {}) {
  const fixture = dirname(dirname(dirname(database)));
  /* Keep startup from reindexing the fixture. */
  const settings = join(fixture, 'musikcube', 'settings.json');
  const preferences = fs.existsSync(settings) ? JSON.parse(fs.readFileSync(settings, 'utf8')) : {};
  preferences.SyncOnStartup = false;
  fs.writeFileSync(settings, JSON.stringify(preferences));

  const env = {
    ...process.env, HOME: fixture, XDG_CONFIG_HOME: fixture, TERM: 'xterm-256color',
    MUSIKCUBE_SEARCH_DATA: dirname(database),
    LC_ALL: process.platform === 'darwin' ? 'en_US.UTF-8' : 'C.UTF-8',
  };
  delete env.LANGUAGE;
  if (pilot) {
    delete env.MUSIKCUBE_SEARCH_CACHE_PAGES;
    delete env.MUSIKCUBE_SEARCH_PREFETCH_PAGES;
    if (cache.capacityOverride !== undefined) env.MUSIKCUBE_SEARCH_CACHE_PAGES = String(cache.capacity);
    if (cache.prefetchOverride !== undefined) env.MUSIKCUBE_SEARCH_PREFETCH_PAGES = String(cache.prefetch);
    env.MUSIKCUBE_SEARCH_CACHE_DIAGNOSTICS = '1';
  }

  const terminal = new Terminal(runtime, env, 18, 140);
  const capture = async (name, lastVisibleTrack) => {
    if (!screenshots) return;
    await terminal.expect(lastVisibleTrack);
    fs.writeFileSync(join(screenshots, `${name}.svg`), terminal.screen.svg());
  };
  try {
    await terminal.expect(pilot ? 'Track Search Pilot' : 'playback is stopped');
    const ready = terminal.processStats();
    let started = performance.now();
    terminal.send('t');
    await terminal.expect('search');
    await terminal.expect(FIRST_TRACK);
    const openMs = performance.now() - started;
    if (!pilot) await terminal.expect(`${FIRST_ALBUM} - 0:10`);
    await capture('01-search', `${FIRST_ALBUM} - Track 08`);

    /* Filter to a different album and scroll beyond the first viewport. */
    started = performance.now();
    terminal.send(FILTER);
    await terminal.expect(FILTERED_FIRST, { absent: FIRST_TRACK });
    const filterMs = performance.now() - started;
    if (terminal.screen.text().includes(FILTERED_LAST)) {
      throw new Error('The last result already fits in the viewport');
    }
    await capture('02-filtered', `${FILTER} - Track 08`);

    started = performance.now();
    terminal.send(DOWN + END);
    await terminal.expect(FILTERED_LAST, { absent: FILTERED_FIRST });
    const scrollMs = performance.now() - started;
    await capture('03-scrolled', FILTERED_LAST);

    const result = { openMs, filterMs, scrollMs };
    if (windows) {
      /* Widen the filter and verify whether Page Down uses a prefetched page. */
      terminal.send(HOME + UP + '\x7f'.repeat(3));
      await terminal.expect(FIRST_TRACK, { absent: FILTERED_LAST });
      if (pilot && cache.prefetch) await terminal.expect('adjacent page ready');
      started = performance.now();
      terminal.send(DOWN + PAGE_DOWN);
      await terminal.expect(FIRST_WINDOW_MISS);
      result.nextMs = performance.now() - started;
      if (pilot) await terminal.expect(cache.prefetch ? 'cache hit' : 'cache miss');

      started = performance.now();
      terminal.send(HOME);
      await terminal.expect(FIRST_TRACK);
      result.warmMs = performance.now() - started;

      started = performance.now();
      terminal.send(END);
      await terminal.expect(DISTANT_TRACK);
      result.jumpMs = performance.now() - started;
      if (pilot && cache.prefetch < DISTANT_PAGE) await terminal.expect('cache miss');
    }

    const finished = terminal.processStats();
    if (finished) {
      result.peakRssKiB = finished.peakRssKiB;
      result.cpuTicks = finished.cpuTicks - ready.cpuTicks;
      result.totalCpuTicks = finished.cpuTicks;
    }
    if (pilot && windows) {
      terminal.send(HOME + 'j\r');
      await terminal.expect(`Selected track: ${FIRST_ALBUM} - Track 02`);
    } else if (!pilot) {
      /* Append the selected search result through its track actions menu. */
      terminal.send('\x1b\r');
      await terminal.expect('play now');
      terminal.send(END + UP + UP + '\r');
      terminal.send('n');
      const queuedTrack = windows ? DISTANT_TRACK : FILTERED_LAST;
      await terminal.expect(queuedTrack, { absent: 'tracks (sorted by' });

      /* Now Playing refreshes from the queue notification after clear. */
      terminal.send('X');
      await terminal.expect('play queue', { absent: queuedTrack });
    }
    terminal.send('\x04');
    await terminal.expectExit();
    return result;
  } catch (error) {
    throw new Error(`${error.message}\n${terminal.screen.text()}\nPTY output tail ${JSON.stringify(terminal.transcript.toString())}`);
  } finally {
    await terminal.close();
  }
}

async function main() {
  const { values } = parseArgs({
    options: {
      'build-dir': { type: 'string', default: SOURCE },
      'bin-dir': { type: 'string', default: join(SOURCE, 'bin') },
      screenshots: { type: 'string' },
      tracks: { type: 'string', default: String(TRACK_COUNT) },
      runs: { type: 'string', default: '1' },
      compare: { type: 'boolean' },
      windows: { type: 'boolean' },
      'pilot-control': { type: 'string' },
      'cache-pages': { type: 'string' },
      'prefetch-pages': { type: 'string' },
      help: { type: 'boolean' },
    },
  });
  if (values.help) {
    console.log('Usage: node tests/tui/track-search.js [--build-dir <dir>] [--bin-dir <dir>] [--screenshots <dir>] [--tracks <count>] [--runs <count>] [--windows] [--compare | --pilot-control <dir>] [--cache-pages <count>] [--prefetch-pages <count>]');
    return;
  }
  if (!['linux', 'darwin'].includes(process.platform)) {
    throw new Error('This test requires script on Linux or macOS');
  }

  const build = resolve(values['build-dir']);
  const tracks = Number(values.tracks);
  const runs = Number(values.runs);
  if (!Number.isSafeInteger(tracks) || tracks < 20 || !Number.isSafeInteger(runs) || runs < 1) {
    throw new Error('Use at least 20 tracks and one run');
  }
  const controlDir = values['pilot-control'];
  const compare = values.compare || controlDir !== undefined;
  if (values.compare && controlDir !== undefined) throw new Error('Choose one comparison mode');
  const windows = Boolean(values.windows || compare);
  if (windows && tracks < 100) throw new Error('The viewport scenario requires at least 100 tracks');
  const capacity = Number(values['cache-pages'] ?? '3');
  const prefetch = Number(values['prefetch-pages'] ?? Math.min(1, capacity - 1));
  if (!Number.isSafeInteger(capacity) || capacity < 1 ||
      !Number.isSafeInteger(prefetch) || prefetch < 0 || prefetch >= capacity) {
    throw new Error('Cache pages must be positive and prefetch pages must be smaller than cache pages');
  }
  const cache = {
    capacity, prefetch,
    capacityOverride: values['cache-pages'], prefetchOverride: values['prefetch-pages'],
  };
  const tickMs = await cpuMillisecondsPerTick();
  const work = join(build, 'tests', 'bin');
  fs.mkdirSync(work, { recursive: true });
  const screenshots = values.screenshots ? resolve(values.screenshots) : undefined;
  if (screenshots) fs.mkdirSync(screenshots, { recursive: true });
  const runtime = fs.mkdtempSync(join(work, 'tui-'));

  try {
    stageRuntime(runtime, resolve(values['bin-dir']));
    let control = { name: 'Production', runtime, pilot: false };
    if (controlDir !== undefined) {
      const controlRuntime = join(runtime, 'control');
      stagePilot(controlRuntime, resolve(controlDir));
      control = { name: 'ABI0 control', runtime: controlRuntime, pilot: true };
    }
    const variants = [control];
    if (compare) {
      const pilot = join(runtime, 'pilot');
      stagePilot(pilot, join(build, 'bin', 'abi0'));
      variants.push({ name: 'ABI0 pilot', runtime: pilot, pilot: true });
    }
    const started = performance.now();
    const database = await seedLibrary(runtime, build, tracks);
    const fixture = dirname(dirname(dirname(database)));
    const seedMs = performance.now() - started;
    const results = await sampleVariants(variants, runs, async variant => {
      try {
        return await checkTrackSearch(variant.runtime, database,
          variant.pilot ? undefined : screenshots, { pilot: variant.pilot, windows, cache });
      } finally {
        fs.rmSync(join(fixture, 'musikcube', 'session.json'), { force: true });
      }
    });
    console.log(`Track search passed with ${tracks.toLocaleString('en-US')} tracks across ${runs} runs.`);
    console.log(`Generate and index ${(seedMs / 1000).toFixed(2)} s`);
    if (compare) console.log(`ABI0 cache: ${capacity} pages, ${prefetch} prefetched in the direction of travel`);
    printMetrics(results, [
      ['Open', 'openMs', 'ms'], ['Filter', 'filterMs', 'ms'], ['Scroll', 'scrollMs', 'ms'],
      ['Next window', 'nextMs', 'ms'], ['Back to first window', 'warmMs', 'ms'],
      ['Jump to last window', 'jumpMs', 'ms'], ['Peak RSS', 'peakRssKiB', 'KiB'],
      ['Search scenario CPU', 'cpuTicks', 'ms', tickMs],
      ['Startup + search CPU', 'totalCpuTicks', 'ms', tickMs],
    ]);
    if (screenshots && !control.pilot) console.log(`Screenshots saved to ${screenshots}`);
  } finally {
    fs.rmSync(runtime, { recursive: true, force: true });
  }
}

main().catch(error => {
  console.error(error.message);
  process.exitCode = 1;
});
