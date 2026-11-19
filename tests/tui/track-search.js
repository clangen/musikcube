const { spawn } = require('node:child_process');
const fs = require('node:fs');
const { dirname, join, resolve } = require('node:path');
const { performance } = require('node:perf_hooks');
const { parseArgs } = require('node:util');
const { Terminal } = require('./terminal');

const SOURCE = resolve(__dirname, '../..');
const TRACK_COUNT = 10000;
const FILTER = 'Artist 001 - Album 02';
const FIRST_ALBUM = 'Artist 001 - Album 01';
const FIRST_TRACK = `${FIRST_ALBUM} - Track 01`;
const FILTERED_FIRST = `${FILTER} - Track 01`;
const FILTERED_LAST = `${FILTER} - Track 10`;

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

async function seedLibrary(runtime, build) {
  const log = join(runtime, 'indexer.log');
  const output = fs.openSync(log, 'w');
  try {
    const child = spawn(process.execPath, [
      join(SOURCE, 'script/seed-test-library.js'),
      '--tracks', String(TRACK_COUNT),
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
  return dirname(dirname(dirname(database)));
}

async function checkTrackSearch(runtime, fixture, screenshots) {
  /* Keep startup from reindexing the fixture. */
  const settings = join(fixture, 'musikcube', 'settings.json');
  const preferences = fs.existsSync(settings) ? JSON.parse(fs.readFileSync(settings, 'utf8')) : {};
  preferences.SyncOnStartup = false;
  fs.writeFileSync(settings, JSON.stringify(preferences));

  const env = {
    ...process.env, HOME: fixture, XDG_CONFIG_HOME: fixture, TERM: 'xterm-256color',
    LC_ALL: process.platform === 'darwin' ? 'en_US.UTF-8' : 'C.UTF-8',
  };
  delete env.LANGUAGE;

  const terminal = new Terminal(runtime, env, 18, 140);
  const capture = async (name, lastVisibleTrack) => {
    if (!screenshots) return;
    await terminal.expect(lastVisibleTrack);
    fs.writeFileSync(join(screenshots, `${name}.svg`), terminal.screen.svg());
  };
  try {
    await terminal.expect('playback is stopped');
    let started = performance.now();
    terminal.send('t');
    await terminal.expect('search');
    await terminal.expect(FIRST_TRACK);
    const openMs = performance.now() - started;
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
    terminal.send('\x1bOB\x1bOF');
    await terminal.expect(FILTERED_LAST, { absent: FILTERED_FIRST });
    const scrollMs = performance.now() - started;
    await capture('03-scrolled', FILTERED_LAST);

    terminal.send('\x04');
    await terminal.expectExit();
    return { openMs, filterMs, scrollMs };
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
      help: { type: 'boolean' },
    },
  });
  if (values.help) {
    console.log('Usage: node tests/tui/track-search.js [--build-dir <dir>] [--bin-dir <dir>] [--screenshots <dir>]');
    return;
  }
  if (!['linux', 'darwin'].includes(process.platform)) {
    throw new Error('This test requires script on Linux or macOS');
  }

  const build = resolve(values['build-dir']);
  const work = join(build, 'tests', 'bin');
  fs.mkdirSync(work, { recursive: true });
  const screenshots = values.screenshots ? resolve(values.screenshots) : undefined;
  if (screenshots) fs.mkdirSync(screenshots, { recursive: true });
  const runtime = fs.mkdtempSync(join(work, 'tui-'));

  try {
    stageRuntime(runtime, resolve(values['bin-dir']));
    const started = performance.now();
    const fixture = await seedLibrary(runtime, build);
    const seedMs = performance.now() - started;
    const { openMs, filterMs, scrollMs } = await checkTrackSearch(runtime, fixture, screenshots);
    console.log(`Track search passed with ${TRACK_COUNT.toLocaleString('en-US')} tracks.`);
    console.log(`Generate and index ${(seedMs / 1000).toFixed(2)} s`);
    console.log(`Keystroke to visible result: open ${openMs.toFixed(1)} ms, filter ${filterMs.toFixed(1)} ms, scroll ${scrollMs.toFixed(1)} ms`);
    if (screenshots) console.log(`Screenshots saved to ${screenshots}`);
  } finally {
    fs.rmSync(runtime, { recursive: true, force: true });
  }
}

main().catch(error => {
  console.error(error.message);
  process.exitCode = 1;
});
