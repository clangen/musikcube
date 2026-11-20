/* Protect the screen model independently of the app: an incorrect terminal
   parser can make a good UI fail or allow a broken UI scenario to pass. */

const assert = require('node:assert/strict');
const { test } = require('node:test');
const { Screen } = require('../support/screen');

/* Feed explicit byte boundaries, including boundaries inside UTF-8 and CSI tokens. */
function feedChunks(screen, text, chunkSize = 1) {
  const bytes = Buffer.from(text);
  for (let offset = 0; offset < bytes.length; offset += chunkSize) {
    screen.feed(bytes.subarray(offset, offset + chunkSize));
  }
}

/* Reads can split every UTF-8 character; the visible text must remain intact. */
test('split UTF-8 and wide/combining glyphs keep cell positions', () => {
  const screen = new Screen(3, 12);
  feedChunks(screen, 'A音e\u0301B');
  assert.equal(screen.text().split('\n')[0], 'A音e\u0301B');
  assert.equal(screen.column, 5);
  assert.equal(screen.cells[0][2], '');
  assert.equal(screen.cells[0][3], 'e\u0301');
});

/* An incomplete escape must wait for its final byte rather than printing junk. */
test('split cursor and OSC sequences are decoded incrementally', () => {
  const screen = new Screen(3, 12);
  feedChunks(screen, '\x1b]0;ignored title\x07\x1b[2;4HX');
  assert.equal(screen.cells[1][3], 'X');
  assert.equal(screen.pending, '');
  assert.equal(screen.text(), '\n   X\n');
});

/* Relative addressing, carriage returns and backspace must address rendered cells. */
test('cursor movement overwrites the intended cells', () => {
  const screen = new Screen(3, 10);
  feedChunks(screen, 'hello\rY\x1b[3G!\x1b[1B\x1b[2DZ\bQ');
  assert.equal(screen.text(), 'Ye!lo\n Q\n');
});

/* Rendering a new view must remove old content instead of matching transcript text. */
test('line and screen erase remove stale visible text', () => {
  const screen = new Screen(3, 12);
  feedChunks(screen, 'old content\x1b[1;4H\x1b[Knew');
  assert.equal(screen.text(), 'oldnew\n\n');
  feedChunks(screen, '\x1b[2J\x1b[Hfresh');
  assert.equal(screen.text(), 'fresh\n\n');
});

/* A write to the continuation half must not leave an orphaned CJK glyph. */
test('overwriting and erasing a wide glyph clears both halves', () => {
  const screen = new Screen(2, 8);
  feedChunks(screen, '音B\x1b[1;2HX');
  assert.equal(screen.text(), ' XB\n');
  feedChunks(screen, '\x1b[H音\x1b[1;2H\x1b[K');
  assert.equal(screen.text(), '\n');
});

/* Line feed scrolls only the selected list region, keeping fixed rows untouched. */
test('scroll regions preserve headers and footers', () => {
  const screen = new Screen(4, 8);
  feedChunks(screen, 'header\x1b[2;1Hone\x1b[3;1Htwo\x1b[4;1Hfooter');
  feedChunks(screen, '\x1b[2;3r\x1b[3;1H\nnew');
  assert.equal(screen.text(), 'header\ntwo\nnew\nfooter');
  feedChunks(screen, '\x1b[99;100r\x1b[S');
  assert.equal(screen.text(), 'header\nnew\n\nfooter');
});

/* Curses uses both DEC saved cursors and alternate-screen resets. */
test('saved cursors and alternate screen reset do not leak old state', () => {
  const screen = new Screen(3, 10);
  feedChunks(screen, '\x1b[2;3H\x1b7\x1b[HX\x1b8Y');
  assert.equal(screen.cells[1][2], 'Y');
  feedChunks(screen, '\x1b[?1049hZ');
  assert.equal(screen.text(), 'Z\n\n');
});

/* SGR colors and inverse-video must survive into diagnostic SVG captures. */
test('style changes and SVG escaping preserve diagnostic content', () => {
  const screen = new Screen(2, 12);
  feedChunks(screen, '\x1b[31m<&>\x1b[7mR\x1b[0mN');
  assert.deepEqual(screen.styles[0][0], ['#800000', '#171717']);
  assert.deepEqual(screen.styles[0][3], ['#171717', '#800000']);
  assert.deepEqual(screen.styles[0][4], ['#ddd', '#171717']);
  assert.match(screen.svg(), /&lt;&amp;&gt;/);
  assert.match(screen.svg(), /fill="#800000"/);
});

/* A wide glyph cannot start in the final column; it wraps as one unit. */
test('wide glyph wrapping keeps its continuation on the same row', () => {
  const screen = new Screen(3, 4);
  feedChunks(screen, 'abc音Z');
  assert.equal(screen.text(), 'abc\n音Z\n');
  assert.equal(screen.column, 3);
});

/* Resizing a live PTY changes the model dimensions before the app repaints. */
test('resize preserves intersecting content and clamps cursor state', () => {
  const screen = new Screen(3, 10);
  feedChunks(screen, 'hello\x1b[3;10H');
  screen.resize(2, 6);
  assert.equal(screen.text(), 'hello\n');
  assert.equal(screen.row, 1);
  assert.equal(screen.column, 6);
  screen.resize(4, 14);
  assert.equal(screen.cells.length, 4);
  assert.equal(screen.cells[0].length, 14);
  assert.equal(screen.scrollBottom, 3);
  assert.throws(() => screen.resize(0, 10), RangeError);
});
