/* Check that Cube applies a locale resource's overlay width. */
const fs = require('node:fs');
const { join } = require('node:path');
const { DOWN } = require('../support/catalog');

const SORT_OVERLAY_WIDTH = 60;

function stageLocale(runtime) {
  /* Override the width in the isolated runtime. */
  const locales = join(runtime, 'locales');
  const resource = JSON.parse(fs.readFileSync(join(locales, 'en_US.json'), 'utf8'));
  resource.integers.track_search_sort_order_width = SORT_OVERLAY_WIDTH;
  fs.unlinkSync(locales);
  fs.mkdirSync(locales);
  fs.writeFileSync(join(locales, 'en_US.json'), JSON.stringify(resource));
}

async function checkLocale(terminal) {
  terminal.send(DOWN + '\x1bs');
  await terminal.expect('select sort order');
  const titleRow = terminal.screen.text().split('\n').find(row => row.includes('select sort order'));
  const left = Math.floor(terminal.screen.columns / 2) - SORT_OVERLAY_WIDTH / 2;
  const right = left + SORT_OVERLAY_WIDTH - 1;
  if (!/[│x]/.test(titleRow[left]) || titleRow[right] !== titleRow[left]) {
    throw new Error('Sort overlay did not use the locale-specific width');
  }
  terminal.send('\x1b');
  await terminal.expect('tracks (sorted by', { absent: 'select sort order' });
}

module.exports = { stageLocale, checkLocale };

if (require.main === module) require('../support/run-scenario').run('locale');
