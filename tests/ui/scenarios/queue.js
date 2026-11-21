/* Add a track through Search, then verify the queue and playback. */
const { END, UP } = require('../support/catalog');

async function appendToQueue(terminal, queuedTrack) {
  /* Append the selected search result through its track actions menu. */
  terminal.send('\x1b\r');
  await terminal.expect('play now');
  terminal.send(END + UP + UP + '\r');
  terminal.send('n');
  await terminal.expect(queuedTrack, { absent: 'tracks (sorted by' });
}

async function checkQueue(terminal, queuedTrack) {
  terminal.send('X');
  await terminal.expect('play queue', { absent: queuedTrack });
}

module.exports = { appendToQueue, checkQueue };

if (require.main === module) require('../support/run-scenario').run('queue');
