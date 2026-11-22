/* Measure track search, filtering, and paging. */
const { performance } = require('node:perf_hooks');
const { FILTER, FIRST_ALBUM, FIRST_TRACK, FILTERED_FIRST, FILTERED_LAST, FIRST_WINDOW_MISS,
  DISTANT_TRACK, DOWN, UP, END, HOME, PAGE_DOWN, DISTANT_PAGE } = require('../support/catalog');

async function checkSearch(terminal, { pilot, windows, cache, openSearch, capture }) {
  const ready = terminal.processStats();
  let started = performance.now();
  terminal.send(openSearch);
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
    if (finished.cpuTicks !== undefined) {
      result.cpuTicks = finished.cpuTicks - ready.cpuTicks;
      result.totalCpuTicks = finished.cpuTicks;
    } else {
      result.cpuMs = finished.cpuMs - ready.cpuMs;
      result.totalCpuMs = finished.cpuMs;
    }
  }
  if (pilot && windows) {
    terminal.send(HOME + 'j\r');
    await terminal.expect(`Selected track: ${FIRST_ALBUM} - Track 02`);
  }
  return result;
}

async function checkSortOrder(terminal) {
  terminal.send('\x1bs');
  await terminal.expect('select sort order');
  terminal.send(DOWN + '\r');
  await terminal.expect('tracks (sorted by track title)', { absent: 'select sort order' });
}

module.exports = { checkSearch, checkSortOrder };

if (require.main === module) require('../support/run-scenario').run('search');
