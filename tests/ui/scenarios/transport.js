/* Hold the first audio buffer and observe the transport through startup and controls. */
async function checkTransport(terminal, queuedTrack) {
  const states = [];
  const directive = /\$(?:title|artist|album|state)\b/;
  let leakedDirective = false;
  const observe = () => {
    const screen = terminal.screen.text();
    leakedDirective ||= directive.test(screen) || directive.test(terminal.transcript.toString());
    const line = screen.split('\n').find(row => /\bshuffle\s*$/.test(row));
    const status = line?.match(/^\s*(buffering|paused|playing)\b/);
    if (status) {
      const state = line.includes(queuedTrack) ? status[1] : 'missing metadata';
      if (states.at(-1) !== state) states.push(state);
    }
  };
  terminal.events.on('update', observe);
  try {
    terminal.send('\r');
    await terminal.expect('buffering ' + queuedTrack);
    await terminal.expect('playing ' + queuedTrack);
  } finally {
    terminal.events.off('update', observe);
  }
  if (states.join(',') !== 'buffering,playing') {
    throw new Error('Unexpected transport transition: ' + states.join(' → '));
  }
  if (leakedDirective) throw new Error('Transport displayed an unresolved format directive');

  terminal.send('\x10');
  await terminal.expect('paused ' + queuedTrack);
  terminal.send('\x10');
  await terminal.expect('playing ' + queuedTrack);
  terminal.send('\x18');
  await terminal.expect('playback is stopped');
}

module.exports = { checkTransport };

if (require.main === module) require('../support/run-scenario').run('transport');
