/* Verify that visible lyrics follow playback and clear on stop. */
const { DOWN, FIRST_TRACK, SECOND_TRACK } = require('../support/catalog');

async function checkLyrics(terminal) {
  terminal.send(DOWN + '\r');
  await terminal.expect('playing ' + FIRST_TRACK);
  terminal.send('\x0c');
  await terminal.expect(`lyrics for '${FIRST_TRACK}' by 'Artist 001'`);
  await terminal.expect('Fixture lyrics for track 1');

  terminal.send('l');
  await terminal.expect(`lyrics for '${SECOND_TRACK}' by 'Artist 001'`);
  await terminal.expect('Fixture lyrics for track 2', { absent: 'Fixture lyrics for track 1' });

  terminal.send('\x18');
  await terminal.expect("can't show lyrics because nothing is playing!");
}

module.exports = { checkLyrics };

if (require.main === module) require('../support/run-scenario').run('lyrics');
