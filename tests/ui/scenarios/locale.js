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
  const left = Math.floor(terminal.screen.columns / 2) - SORT_OVERLAY_WIDTH / 2;
  const right = left + SORT_OVERLAY_WIDTH - 1;
  /* the PTY can deliver the title before the rest of its frame. */
  await terminal.waitFor(() => {
    const row = terminal.screen.text().split('\n').find(line => line.includes('select sort order'));
    return row && /[│x]/.test(row[left]) && row[right] === row[left];
  }, 'sort overlay with locale-specific width');
  terminal.send('\x1b');
  await terminal.expect('tracks (sorted by', { absent: 'select sort order' });
}

module.exports = { stageLocale, checkLocale };

if (require.main === module) require('../support/run-scenario').run('locale');
