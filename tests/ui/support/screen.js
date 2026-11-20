/* Small screen model for the terminal sequences emitted by the TUI scenarios.
   Turns PTY output into searchable cells and diagnostic SVG captures, allowing
   tests to check what the user sees rather than searching an append-only log. */

const { StringDecoder } = require('node:string_decoder');

const FOREGROUND = '#ddd';
const BACKGROUND = '#171717';
const BASIC = ['#000', '#800000', '#008000', '#808000', '#000080', '#800080', '#008080', '#c0c0c0',
  '#808080', '#f00', '#0f0', '#ff0', '#00f', '#f0f', '#0ff', '#fff'];
const BOX = { l: '┌', k: '┐', m: '└', j: '┘', q: '─', x: '│', t: '├', u: '┤', v: '┴', w: '┬', n: '┼' };
const XML = { '&': '&amp;', '<': '&lt;', '>': '&gt;' };

/* Treat combining/format characters as zero-width and the ordinary CJK/fullwidth
   ranges as two cells, matching the app's wcwidth-style layout. Ambiguous glyphs
   stay narrow; emoji grapheme/ZWJ presentation is not a terminal-layout oracle. */
function cellWidth(char) {
  if (/^[\p{Mark}\p{Default_Ignorable_Code_Point}]$/u.test(char)) {
    return 0;
  }

  const point = char.codePointAt(0);
  return point >= 0x1100 && (point <= 0x115f || point === 0x2329 || point === 0x232a ||
    (point >= 0x2e80 && point <= 0xa4cf && point !== 0x303f) ||
    (point >= 0xac00 && point <= 0xd7a3) || (point >= 0xf900 && point <= 0xfaff) ||
    (point >= 0xfe10 && point <= 0xfe19) || (point >= 0xfe30 && point <= 0xfe6f) ||
    (point >= 0xff00 && point <= 0xff60) || (point >= 0xffe0 && point <= 0xffe6) ||
    (point >= 0x20000 && point <= 0x3fffd)) ? 2 : 1;
}

/* Translate ANSI palette indices into SVG colors for diagnostic screen captures. */
function color(index) {
  if (index < 16) return BASIC[index];
  if (index >= 232) {
    const gray = 8 + (index - 232) * 10;
    return `rgb(${gray},${gray},${gray})`;
  }
  const cube = index - 16;
  const levels = [0, 95, 135, 175, 215, 255];
  return `rgb(${levels[Math.floor(cube / 36)]},${levels[Math.floor(cube / 6) % 6]},${levels[cube % 6]})`;
}

class Screen {
  /* Create a fixed-size terminal cell grid and incremental UTF-8 decoder. */
  constructor(rows, columns) {
    this.rows = rows;
    this.columns = columns;
    this.resetStyle();
    this.clear();

    /* PTY reads can end inside a UTF-8 character; decoding each read as a whole
       string would corrupt text and make screen assertions unreliable. */
    this.decoder = new StringDecoder('utf8');
    this.pending = '';
  }

  /* Reset cells, cursor, scrolling region, and saved cursor for a fresh terminal screen. */
  clear() {
    this.cells = Array.from({ length: this.rows }, () => Array(this.columns).fill(' '));
    this.styles = Array.from({ length: this.rows }, () => Array(this.columns));
    this.row = 0;
    this.column = 0;
    this.scrollTop = 0;
    this.scrollBottom = this.rows - 1;
    this.savedCursor = [0, 0];
  }

  /* Decode PTY chunks and apply complete terminal tokens, retaining incomplete UTF-8/escape sequences. */
  feed(data) {
    this.pending += this.decoder.write(data);

    /* Preserve incomplete UTF-8 and escape sequences across PTY reads. */
    const tokenPattern = /\x1b(?:\[([0-?]*)[ -/]*([@-~])|\][^\x07\x1b]*(?:\x07|\x1b\\)|[()][ -~]|[^\[\]()])|[^\x1b]/uy;
    let offset = 0;
    for (let match; (match = tokenPattern.exec(this.pending));) {
      offset = tokenPattern.lastIndex;
      const [token, params, operation] = match;

      if (operation && /^[\d;]*$/.test(params)) {
        this.control(operation, params.split(';').map(Number));
      } else if (token === '\x1b[?1049h') {
        this.clear();
      } else if (token === '\x1b7') {
        this.savedCursor = [this.row, this.column];
      } else if (token === '\x1b8') {
        [this.row, this.column] = this.savedCursor;
      } else if (token === '\r') {
        this.column = 0;
      } else if (token === '\n') {
        this.lineFeed();
      } else if (token === '\b') {
        this.column = Math.max(0, this.column - 1);
      } else if (token.startsWith('\x1b(')) {
        this.boxDrawing = token === '\x1b(0';
      } else if (!/^[\x00-\x1f\x7f]/.test(token)) {
        this.write(this.boxDrawing ? BOX[token] || token : token);
      }
    }
    this.pending = this.pending.slice(offset);
  }

  /* Place one decoded character at the cursor and apply the model's cell wrapping behavior. */
  write(char) {
    const width = cellWidth(char);
    if (width === 0) {
      let row = this.row;
      let column = this.column - 1;
      if (column < 0 && row > 0) {
        --row;
        column = this.columns - 1;
      }
      while (column > 0 && this.cells[row][column] === '') {
        --column;
      }
      if (column >= 0) {
        this.cells[row][column] += char;
      }
      return;
    }

    if (this.column + width > this.columns) {
      this.lineFeed();
      this.column = 0;
    }

    this.erase(this.row, this.column, this.column + width);
    this.cells[this.row][this.column] = char;
    this.styles[this.row][this.column] = this.style;
    if (width === 2) {
      this.cells[this.row][this.column + 1] = '';
      this.styles[this.row][this.column + 1] = this.style;
    }
    this.column += width;
  }

  /* Advance within the scroll region, scrolling its contents only at its bottom. */
  lineFeed() {
    if (this.row === this.scrollBottom) {
      this.scrollUp(1);
    } else {
      this.row = Math.min(this.row + 1, this.rows - 1);
    }
  }

  /* Preserve intersecting cells while a real PTY resize causes the app to repaint. */
  resize(rows, columns) {
    if (!Number.isSafeInteger(rows) || rows < 1 || !Number.isSafeInteger(columns) || columns < 2) {
      throw new RangeError('Terminal dimensions must be positive integers with at least two columns');
    }

    this.cells = Array.from({ length: rows }, (_, row) =>
      Array.from({ length: columns }, (_, column) => this.cells[row]?.[column] ?? ' '));
    this.styles = Array.from({ length: rows }, (_, row) =>
      Array.from({ length: columns }, (_, column) => this.styles[row]?.[column]));
    this.rows = rows;
    this.columns = columns;
    this.row = Math.min(this.row, rows - 1);
    this.column = Math.min(this.column, columns);
    this.scrollTop = 0;
    this.scrollBottom = rows - 1;
    this.savedCursor = [Math.min(this.savedCursor[0], rows - 1), Math.min(this.savedCursor[1], columns - 1)];
  }

  /* Restore default foreground/background and clear inverse-video styling. */
  resetStyle() {
    this.foreground = FOREGROUND;
    this.background = BACKGROUND;
    this.reverse = false;
    this.style = undefined;
  }

  /* Apply ANSI color and inverse-video attributes to subsequent cell writes. */
  setStyle(args) {
    /* Apply the colors and inverse-video attribute emitted by the app. */
    for (let i = 0; i < args.length; ++i) {
      const code = args[i];
      if (code === 0) this.resetStyle();
      else if (code === 7 || code === 27) this.reverse = code === 7;
      else if (code === 39) this.foreground = FOREGROUND;
      else if (code === 49) this.background = BACKGROUND;
      else if (code >= 30 && code <= 37) this.foreground = color(code - 30);
      else if (code >= 40 && code <= 47) this.background = color(code - 40);
      else if (code >= 90 && code <= 97) this.foreground = color(code - 82);
      else if (code >= 100 && code <= 107) this.background = color(code - 92);
      else if (code === 38 || code === 48) {
        const target = code === 38 ? 'foreground' : 'background';
        const mode = args[++i];
        if (mode === 5) this[target] = color(args[++i]);
        else if (mode === 2) {
          this[target] = `rgb(${args.slice(i + 1, i + 4).join(',')})`;
          i += 3;
        }
      }
    }

    this.style = this.reverse
      ? [this.background, this.foreground]
      : [this.foreground, this.background];
  }

  /* Blank a cell range using the currently active style, preserving the rest of the row. */
  erase(row, start = 0, end = this.columns) {
    /* An overwrite touching either half of a wide glyph clears the whole glyph. */
    if (this.cells[row][start] === '' && start > 0) {
      --start;
    }
    if (end < this.columns && this.cells[row][end] === '') {
      ++end;
    }
    this.cells[row].fill(' ', start, end);
    this.styles[row].fill(this.style, start, end);
  }

  /* Scroll only the active terminal region so fixed headers and input rows remain intact. */
  scrollUp(count) {
    /* Curses scrolls the list independently of the input and transport. */
    for (let row = this.scrollTop; row <= this.scrollBottom; ++row) {
      if (row + count <= this.scrollBottom) {
        this.cells[row] = this.cells[row + count];
        this.styles[row] = this.styles[row + count];
      } else {
        this.cells[row] = Array(this.columns).fill(' ');
        this.styles[row] = Array(this.columns).fill(this.style);
      }
    }
  }

  /* Apply supported cursor, style, scrolling, and erase commands while keeping coordinates in bounds. */
  control(operation, args) {
    const [first, second] = args;
    const count = first || 1;

    /* These are the cursor/style/erase operations emitted by the current
       scenarios; unsupported terminal features are not a layout oracle. */
    switch (operation) {
      case 'H':
      case 'f':
        this.row = count - 1;
        this.column = (second || 1) - 1;
        break;
      case 'd': this.row = count - 1; break;
      case 'G': this.column = count - 1; break;
      case 'A': this.row -= count; break;
      case 'B': this.row += count; break;
      case 'C': this.column += count; break;
      case 'D': this.column -= count; break;

      case 'm': this.setStyle(args); break;

      case 'r':
        if (count >= (second || this.rows) || (second || this.rows) > this.rows) {
          break;
        }
        this.scrollTop = count - 1;
        this.scrollBottom = (second || this.rows) - 1;
        this.row = 0;
        this.column = 0;
        break;
      case 'S': this.scrollUp(count); break;

      case 'J':
        if (first === 2) {
          for (let row = 0; row < this.rows; ++row) this.erase(row);
        } else if (first === 0) {
          this.erase(this.row, this.column);
          for (let row = this.row + 1; row < this.rows; ++row) this.erase(row);
        }
        break;
      case 'K':
        if (first === 0) this.erase(this.row, this.column);
        if (first === 1) this.erase(this.row, 0, this.column + 1);
        if (first === 2) this.erase(this.row);
        break;
      case 'X':
        this.erase(this.row, this.column, this.column + count);
        break;
    }

    /* Malformed or out-of-screen coordinates must not index outside the grid. */
    this.row = Math.min(Math.max(0, this.row), this.rows - 1);
    this.column = Math.min(Math.max(0, this.column), this.columns);
  }

  /* Flatten the current grid into searchable visible text without trailing row padding. */
  text() {
    return this.cells.map(row => row.join('').trimEnd()).join('\n');
  }

  /* Render styled cell runs into a standalone SVG for human inspection of a scenario's screen. */
  svg() {
    const cellWidth = 9;
    const cellHeight = 18;
    const width = this.columns * cellWidth;
    const height = this.rows * cellHeight;

    const lines = [
      `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">`,
      `<rect width="${width}" height="${height}" fill="${BACKGROUND}"/>`,
      '<g font-family="monospace" font-size="15" xml:space="preserve">',
    ];

    /* Emit each run once so spaces and highlight backgrounds keep their cell widths. */
    for (let row = 0; row < this.rows; ++row) {
      for (let start = 0; start < this.columns;) {
        const style = this.styles[row][start];
        let end = start + 1;
        while (end < this.columns && this.styles[row][end] === style) ++end;

        const [foreground, background] = style || [FOREGROUND, BACKGROUND];
        const x = start * cellWidth;
        const y = row * cellHeight;
        const runWidth = (end - start) * cellWidth;
        const text = this.cells[row].slice(start, end).join('').replace(/[&<>]/g, char => XML[char]);
        if (background !== BACKGROUND) {
          lines.push(`<rect x="${x}" y="${y}" width="${runWidth}" height="${cellHeight}" fill="${background}"/>`);
        }
        if (text.trim()) {
          lines.push(`<text x="${x}" y="${y + 14}" fill="${foreground}" textLength="${runWidth}" lengthAdjust="spacingAndGlyphs">${text}</text>`);
        }
        start = end;
      }
    }

    return lines.concat('</g>', '</svg>').join('\n');
  }
}

module.exports = { Screen };
