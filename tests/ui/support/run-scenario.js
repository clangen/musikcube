/* Stage isolated catalogs and run the selected PTY workflow. */

const { spawn } = require('node:child_process');
const fs = require('node:fs');
const { basename, dirname, join, resolve } = require('node:path');
const { performance } = require('node:perf_hooks');
const { parseArgs } = require('node:util');
const { sampleVariants, printMetrics, cpuMillisecondsPerTick } = require('./benchmark');
const { Terminal } = require('./terminal');
const { withRuntime } = require('./artifacts');
const { copyRuntimeLibraries } = require('../../scripts/test-runtime');

const SOURCE = resolve(__dirname, '../../..');
const TRACK_COUNT = 10000;
const isWindows = process.platform === 'win32';
const seederName = isWindows ? 'musikcube_seed_library.exe' : 'musikcube_seed_library';
const { FIRST_TRACK, SECOND_TRACK, FILTERED_LAST, DISTANT_TRACK, DOWN } = require('./catalog');
const { checkSearch, checkSortOrder } = require('../scenarios/track-search');
const { stageLocale, checkLocale } = require('../scenarios/locale');
const { checkLyrics } = require('../scenarios/lyrics');
const { appendToQueue, checkQueue } = require('../scenarios/queue');
const { checkTransport } = require('../scenarios/transport');
const { checkPlaylists } = require('../scenarios/playlists');
const { checkBrowse } = require('../scenarios/browse');
const { checkSettings } = require('../scenarios/settings');

/* Stage the production app/assets with only the plugins required to index the generated fixture. */
function stageRuntime(runtime, binaryDir, { settings, lyrics }) {
  const stage = (source, destination) => {
    if (isWindows) fs.cpSync(source, destination, { recursive: true });
    else fs.symlinkSync(fs.realpathSync(source), destination);
  };
  if (isWindows) copyRuntimeLibraries(binaryDir, runtime);
  for (const name of fs.readdirSync(binaryDir)) {
    if (['lib', 'locales', 'share', 'themes'].includes(name) || name.startsWith('libmusikcore')) {
      stage(join(binaryDir, name), join(runtime, name));
    }
  }

  for (const name of [isWindows ? 'musikcube-cmd.exe' : 'musikcube', seederName]) {
    fs.copyFileSync(join(binaryDir, name), join(runtime, name));
  }

  fs.mkdirSync(join(runtime, 'plugins'));
  for (const name of fs.readdirSync(join(binaryDir, 'plugins'))) {
    if (/^(?:lib)?(?:taglibreader|taglib_plugin|ffmpegdecoder)\./.test(name) ||
        (settings && /^(?:lib)?(?:nullout|supereqdsp|server)\./.test(name)) ||
        (lyrics && /^(?:lib)?nullout\./.test(name))) {
      stage(join(binaryDir, 'plugins', name), join(runtime, 'plugins', name));
    }
  }
}

/* Run the production indexer with a retained log and resolve its generated database path. */
async function seedLibrary(runtime, build, tracks, lyrics) {
  const log = join(runtime, 'indexer.log');
  const output = fs.openSync(log, 'w');
  try {
    const child = spawn(process.execPath, [
      join(SOURCE, 'tests/scripts/seed-test-library.js'),
      '--tracks', String(tracks),
      ...(lyrics ? ['--embedded-lyrics', '--seconds', '8'] : []),
      '--indexer', join(runtime, seederName),
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

/* Stage the optional ABI-only search pilot beside its experimental shared library. */
function stagePilot(runtime, binaryDir) {
  fs.mkdirSync(runtime);
  const library = process.platform === 'darwin' ? 'libmusikcore-abi0.dylib' : 'libmusikcore-abi0.so';
  fs.symlinkSync(fs.realpathSync(join(binaryDir, library)), join(runtime, library));
  fs.copyFileSync(join(binaryDir, 'musikcube_abi_search_pilot'), join(runtime, 'musikcube'));
}

/* Launch the PTY with isolated settings, then run the selected checks. */
async function checkWorkflow(runtime, database, screenshots,
  { pilot = false, windows = false, windowsHost, cache, scenario = 'all', playbackMode = 'gapless' } = {}) {
  const fixture = dirname(dirname(dirname(database)));
  /* Keep startup from reindexing the fixture. */
  const settings = join(fixture, 'musikcube', 'settings.json');
  const preferences = fs.existsSync(settings) ? JSON.parse(fs.readFileSync(settings, 'utf8')) : {};
  preferences.SyncOnStartup = false;
  preferences.FirstRunSettingsDisplayed = true;
  preferences.AutoUpdateCheck = false;
  delete preferences.TrackSearchSortOrder;
  fs.writeFileSync(settings, JSON.stringify(preferences));
  if (scenario === 'transport') {
    const transport = playbackMode === 'crossfade' ? 1 : 0;
    fs.writeFileSync(join(fixture, 'musikcube', 'playback.json'), JSON.stringify({ Transport: transport }));
  }
  const openSearch = !pilot && scenario === 'browse' ? 'T' : 't';
  if (openSearch !== 't') {
    fs.writeFileSync(join(fixture, 'musikcube', 'hotkeys.json'),
      JSON.stringify({ navigate_library_tracks: openSearch }));
  }
  const staged = pilot ? [] : fs.readdirSync(join(runtime, 'plugins'));
  const hasServer = staged.some(name => /^(?:lib)?server\./.test(name));
  if (hasServer) {
    fs.writeFileSync(join(fixture, 'musikcube', 'plugin_musikcubeserver(wss,http).json'),
      JSON.stringify({ websocket_server_enabled: false, http_server_enabled: false }));
  }

  const env = {
    ...process.env, HOME: fixture, XDG_CONFIG_HOME: fixture, TERM: 'xterm-256color',
    APPDATA: fixture, USERPROFILE: fixture,
    MUSIKCUBE_SEARCH_DATA: dirname(database),
    LC_ALL: process.platform === 'darwin' ? 'en_US.UTF-8' : 'C.UTF-8',
  };
  delete env.LANGUAGE;
  if (scenario === 'transport') {
    env.MUSIKCORE_TEST_OUTPUT_DELAY_MS = '900';
  }
  if (pilot) {
    delete env.MUSIKCUBE_SEARCH_CACHE_PAGES;
    delete env.MUSIKCUBE_SEARCH_PREFETCH_PAGES;
    if (cache.capacityOverride !== undefined) env.MUSIKCUBE_SEARCH_CACHE_PAGES = String(cache.capacity);
    if (cache.prefetchOverride !== undefined) env.MUSIKCUBE_SEARCH_PREFETCH_PAGES = String(cache.prefetch);
    env.MUSIKCUBE_SEARCH_CACHE_DIAGNOSTICS = '1';
  }

  const terminal = new Terminal(runtime, env, 18, 140, { windowsHost });

  /* Wait for the final intended row before capturing; an earlier matching
     header alone could yield an SVG of an incompletely repainted viewport. */
  const capture = async (name, lastVisibleTrack) => {
    if (!screenshots) return;
    await terminal.expect(lastVisibleTrack);
    fs.writeFileSync(join(screenshots, `${name}.svg`), terminal.screen.svg());
  };

  try {
    await terminal.expect(pilot ? 'Track Search Pilot' : 'playback is stopped');
    const result = {};

    if (pilot || scenario === 'search' || scenario === 'all') {
      Object.assign(result, await checkSearch(terminal, { pilot, windows, cache, openSearch, capture }));
      if (!pilot && scenario === 'search') await checkSortOrder(terminal);
    } else if (scenario !== 'settings') {
      terminal.send(openSearch);
      await terminal.expect(FIRST_TRACK);
    }

    if (!pilot && ['all', 'queue', 'playlists', 'transport'].includes(scenario)) {
      let queuedTrack = FIRST_TRACK;
      if (scenario === 'all') {
        queuedTrack = windows ? DISTANT_TRACK : FILTERED_LAST;
      } else if (scenario === 'playlists') {
        queuedTrack = SECOND_TRACK;
        terminal.send(DOWN + DOWN);
      } else {
        terminal.send(DOWN);
      }
      await appendToQueue(terminal, queuedTrack);
      if (scenario === 'transport') await checkTransport(terminal, queuedTrack);
      if (scenario === 'queue') await checkQueue(terminal, queuedTrack);
      if (scenario === 'all' || scenario === 'playlists') {
        await checkPlaylists(terminal, queuedTrack);
      }
    }

    if (!pilot && (scenario === 'all' || scenario === 'browse')) {
      await checkBrowse(terminal, { afterPlaylists: scenario === 'all', openSearch });
    }
    if (!pilot && (scenario === 'all' || scenario === 'settings')) {
      await checkSettings(terminal, { staged, hasServer });
    }
    if (!pilot && scenario === 'locale') await checkLocale(terminal);
    if (!pilot && scenario === 'lyrics') await checkLyrics(terminal);

    /* The driver must observe a clean application exit, not just a screen that
       happened to match before a shutdown crash. */
    terminal.send('\x04');
    await terminal.expectExit();
    return result;
  } catch (error) {
    terminal.saveArtifacts(join(runtime, 'failure'));
    throw new Error(`${error.message}\n${terminal.screen.text()}\nPTY output tail ${JSON.stringify(terminal.transcript.toString())}`);
  } finally {
    await terminal.close();
  }
}

/* Create an isolated catalog/runtime, run selected variants, report measurements, and clean up. */
async function main(defaultScenario) {
  const { values } = parseArgs({
    options: {
      'build-dir': { type: 'string', default: SOURCE },
      'bin-dir': { type: 'string', default: join(SOURCE, 'bin') },
      'capture-output': { type: 'string' },
      'conpty-host': { type: 'string' },
      'playback-mode': { type: 'string', default: 'gapless' },
      screenshots: { type: 'string' },
      tracks: { type: 'string', default: ['search', 'all'].includes(defaultScenario) ? String(TRACK_COUNT) : '1000' },
      runs: { type: 'string', default: '1' },
      compare: { type: 'boolean' },
      windows: { type: 'boolean' },
      scenario: { type: 'string', default: defaultScenario },
      'pilot-control': { type: 'string' },
      'cache-pages': { type: 'string' },
      'prefetch-pages': { type: 'string' },
      help: { type: 'boolean' },
    },
  });

  if (values.help) {
    console.log(`Usage: node ${basename(process.argv[1])} [--build-dir <dir>] [--bin-dir <dir>] [--conpty-host <launcher.exe>] [--screenshots <dir>] [--tracks <count>] [--runs <count>] [--windows] [--scenario search|queue|playlists|browse|settings|locale|lyrics|transport|all] [--capture-output <path>] [--playback-mode gapless|crossfade] [--compare | --pilot-control <dir>] [--cache-pages <count>] [--prefetch-pages <count>]`);
    return;
  }
  if (isWindows && !values['conpty-host']) {
    throw new Error('Windows UI scenarios require --conpty-host <launcher.exe>');
  }
  if (isWindows && (values.compare || values['pilot-control'])) {
    throw new Error('Windows builds do not provide the ABI-only search pilot');
  }

  if (!['search', 'queue', 'playlists', 'browse', 'settings', 'locale', 'lyrics', 'transport', 'all'].includes(values.scenario)) {
    throw new Error('Unknown UI scenario: ' + values.scenario);
  }
  if (values.scenario === 'transport' && !values['capture-output']) {
    throw new Error('The transport scenario requires --capture-output');
  }
  if (!['gapless', 'crossfade'].includes(values['playback-mode'])) {
    throw new Error('Choose playback mode gapless or crossfade');
  }

  const build = resolve(values['build-dir']);
  const tracks = Number(values.tracks);
  const runs = Number(values.runs);
  if (!Number.isSafeInteger(tracks) || tracks < 20 || !Number.isSafeInteger(runs) || runs < 1) {
    throw new Error('Use at least 20 tracks and one run');
  }

  /* Comparison mode uses identical generated data for both executables so
     differences reflect the implementation rather than a different catalog. */
  const controlDir = values['pilot-control'];
  const compare = values.compare || controlDir !== undefined;
  if (values.compare && controlDir !== undefined) throw new Error('Choose one comparison mode');
  const windows = Boolean(values.windows || compare);
  if (windows && tracks < 100) throw new Error('The viewport scenario requires at least 100 tracks');

  /* Capacity must leave room for the viewport before any prefetch is enabled. */
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

  /* Keep generated databases, settings, staged assets, and screenshots under
     explicit test roots rather than the user's normal app configuration. */
  const tickMs = await cpuMillisecondsPerTick();
  const work = join(build, 'tests', 'bin');
  fs.mkdirSync(work, { recursive: true });
  const screenshots = values.screenshots ? resolve(values.screenshots) : undefined;
  if (screenshots) fs.mkdirSync(screenshots, { recursive: true });
  const runtime = fs.mkdtempSync(join(work, 'tui-'));

  await withRuntime(runtime, async () => {
    /* Stage each implementation separately to prevent loading the wrong core
       library while retaining the same catalog for comparison runs. */
    const lyrics = values.scenario === 'lyrics';
    stageRuntime(runtime, resolve(values['bin-dir']), { settings: ['all', 'settings'].includes(values.scenario), lyrics });
    if (values.scenario === 'locale') stageLocale(runtime);
    if (values.scenario === 'transport') {
      const output = fs.realpathSync(values['capture-output']);
      const target = join(runtime, 'plugins', basename(output));
      if (isWindows) fs.copyFileSync(output, target);
      else fs.symlinkSync(output, target);
    }
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

    /* Index once outside measured interactions; reseeding per launch would
       dominate runtime and make cache/viewport comparisons harder to interpret. */
    const started = performance.now();
    const database = await seedLibrary(runtime, build, tracks, lyrics);
    const fixture = dirname(dirname(dirname(database)));
    const seedMs = performance.now() - started;

    const results = await sampleVariants(variants, runs, async variant => {
      try {
        return await checkWorkflow(variant.runtime, database,
          variant.pilot ? undefined : screenshots, {
            pilot: variant.pilot, windows, cache, scenario: values.scenario, playbackMode: values['playback-mode'],
            windowsHost: isWindows ? resolve(values['conpty-host']) : undefined,
          });
      } finally {
        fs.rmSync(join(fixture, 'musikcube', 'session.json'), { force: true });
      }
    });

    /* Report functional success separately from descriptive latency samples;
       this runner does not enforce machine-specific performance thresholds. */
    console.log(`UI ${values.scenario} passed with ${tracks.toLocaleString('en-US')} tracks across ${runs} runs.`);
    console.log(`Generate and index ${(seedMs / 1000).toFixed(2)} s`);
    if (compare) console.log(`ABI0 cache: ${capacity} pages, ${prefetch} prefetched in the direction of travel`);
    if (compare || ['search', 'all'].includes(values.scenario)) printMetrics(results, [
      ['Open', 'openMs', 'ms'], ['Filter', 'filterMs', 'ms'], ['Scroll', 'scrollMs', 'ms'],
      ['Next window', 'nextMs', 'ms'], ['Back to first window', 'warmMs', 'ms'],
      ['Jump to last window', 'jumpMs', 'ms'], ['Peak RSS', 'peakRssKiB', 'KiB'],
      ['Search scenario CPU', 'cpuTicks', 'ms', tickMs],
      ['Startup + search CPU', 'totalCpuTicks', 'ms', tickMs],
      ['Search scenario CPU', 'cpuMs', 'ms'],
      ['Startup + search CPU', 'totalCpuMs', 'ms'],
    ]);
    if (screenshots && !control.pilot) console.log(`Screenshots saved to ${screenshots}`);
  });
}

function run(scenario) {
  main(scenario).catch(error => {
    console.error(error.message);
    process.exitCode = 1;
  });
}

module.exports = { run };
