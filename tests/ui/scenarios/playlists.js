/* Save, extend, reload, rename, and delete a playlist. */
const { FIRST_TRACK, DOWN, END, HOME } = require('../support/catalog');

async function checkPlaylists(terminal, queuedTrack) {
  /* Save the queue, then add a track from Search. Reloading must show both
     writes in order, even though they came through different UI paths. */
  const playlistName = `Fixture ${Date.now()}`;
  terminal.send('\x1bs');
  await terminal.expect('save playlist');
  terminal.send('\r');
  await terminal.expect('playlist name');
  terminal.send(playlistName + '\r');
  await terminal.expect('play queue', { absent: 'playlist name' });

  /* Append a second track through Track Search's playlist picker. */
  terminal.send('t');
  await terminal.expect('tracks (sorted by');
  await terminal.expect(FIRST_TRACK);
  terminal.send(DOWN + HOME + DOWN + '\x1b\r');
  await terminal.expect('track actions');
  terminal.send(HOME + DOWN.repeat(3) + '\r');
  await terminal.expect(playlistName);
  terminal.send(END + '\r');
  await terminal.expect('tracks (sorted by', { absent: playlistName });
  terminal.send('n');
  await terminal.expect('play queue', { absent: 'tracks (sorted by' });

  /* Now Playing refreshes from the queue notification after clear. */
  terminal.send('X');
  await terminal.expect('play queue', { absent: queuedTrack });
  terminal.send('\x1bl');
  await terminal.expect(playlistName);
  terminal.send(END + '\r');
  await terminal.waitFor(() => {
    const screen = terminal.screen.text();
    const first = screen.indexOf(queuedTrack);
    return !screen.includes('load playlist') && first >= 0 && screen.indexOf(FIRST_TRACK) > first;
  }, 'both saved tracks in queue order');

  /* Rename/delete exercise separate modal focus transitions; checking the
     resulting names prevents an unnoticed picker action on the wrong row. */
  terminal.send('\x1br');
  await terminal.expect('rename playlist');
  terminal.send('\r');
  await terminal.expect('new playlist name');
  terminal.send(END + ' revised\r');
  await terminal.expect('play queue', { absent: 'new playlist name' });

  terminal.send('\x1bx');
  await terminal.expect('delete playlist');
  await terminal.expect(playlistName + ' revised');
  terminal.send('\r');
  await terminal.expect('are you sure you want to delete');
  terminal.send('\r');
  await terminal.expect('play queue', { absent: 'are you sure you want to delete' });
  terminal.send('\x1bl');
  await terminal.expect("you haven't saved any playlists yet!");
  terminal.send('\r');
}

module.exports = { checkPlaylists };

if (require.main === module) require('../support/run-scenario').run('playlists');
