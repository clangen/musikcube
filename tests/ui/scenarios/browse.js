/* Exercise library navigation and actions on tracks, albums, and playlists. */
const { FIRST_ALBUM, FIRST_TRACK, FILTERED_FIRST, DOWN, UP, END, HOME } = require('../support/catalog');

async function checkBrowse(terminal, { afterPlaylists, openSearch }) {
  if (!afterPlaylists) terminal.send(DOWN);
  terminal.send('`');
  await terminal.expect('debug logs');
  await terminal.expect('LocalLibrary');
  terminal.send('a');
  if (afterPlaylists) {
    /* Playlist checks leave a loaded queue and the queue view selected. */
    await terminal.expect('play queue');
    terminal.send('X');
    await terminal.expect('play queue', { absent: FIRST_TRACK });
    terminal.send(openSearch);
    await terminal.expect(FIRST_TRACK);
  }
  await terminal.expect('tracks (sorted by');
  terminal.send('\x12');
  await terminal.expect('metadata indexer');
  terminal.send('\x1b');
  await terminal.expect('tracks (sorted by', { absent: 'metadata indexer' });

  terminal.send(DOWN + DOWN);
  terminal.send('r');
  await terminal.expect('rate track');
  terminal.send('5');
  await terminal.expect('tracks (sorted by', { absent: 'rate track' });

  /* The album divider queues the whole album through the typed category query. */
  terminal.send(HOME + '\r');
  await terminal.expect('album actions');
  terminal.send(END + UP + '\r');
  await terminal.expect('tracks (sorted by', { absent: 'album actions' });
  terminal.send('n');
  await terminal.expect(FIRST_TRACK, { absent: 'tracks (sorted by' });
  await terminal.expect(`${FIRST_ALBUM} - Track 10`);
  terminal.send('X');
  await terminal.expect('play queue', { absent: FIRST_TRACK });

  /* Switch album selections before the first one has time to publish a refresh. */
  terminal.send('b2');
  await terminal.expect('albums');
  terminal.send('6');
  await terminal.expect('select category');
  terminal.send('\x1b');
  await terminal.expect('albums', { absent: 'select category' });
  terminal.send('\x06');
  await terminal.expect('search');
  terminal.send('\r' + HOME);
  await terminal.expect(FIRST_TRACK);
  terminal.send(DOWN + UP + DOWN);
  await terminal.expect(FILTERED_FIRST, { absent: FIRST_TRACK });

  terminal.send('d');
  await terminal.expect('tracks in');
  await terminal.expect(FIRST_TRACK);

  terminal.send('n');
  await terminal.expect('play queue', { absent: 'tracks in' });
  terminal.send('X');
  await terminal.expect('play queue', { absent: FIRST_TRACK });

  /* Exercise the Browse playlist commands through their public API callbacks. */
  terminal.send('b5');
  await terminal.expect('playlists');
  terminal.send('\x1bn');
  await terminal.expect('new playlist name');
  terminal.send('Browse fixture\r');
  await terminal.expect('Browse fixture', { absent: 'new playlist name' });
  terminal.send('\x06');
  await terminal.expect('search');
  terminal.send('\r' + HOME);
  terminal.send('\x1br');
  await terminal.expect('new playlist name');
  terminal.send(END + ' revised\r');
  await terminal.expect('Browse fixture revised', { absent: 'new playlist name' });

}

module.exports = { checkBrowse };

if (require.main === module) require('../support/run-scenario').run('browse');
