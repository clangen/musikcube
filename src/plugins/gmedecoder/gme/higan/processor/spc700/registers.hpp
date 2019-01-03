struct flag_t {
  bool n, v, p, b, h, i, z, c;

  inline operator unsigned() const {
    return (n << 7) | (v << 6) | (p << 5) | (b << 4)
         | (h << 3) | (i << 2) | (z << 1) | (c << 0);
  }

  inline unsigned operator=(uint8_t data) {
    n = data & 0x80; v = data & 0x40; p = data & 0x20; b = data & 0x10;
    h = data & 0x08; i = data & 0x04; z = data & 0x02; c = data & 0x01;
    return data;
  }

  inline unsigned operator|=(uint8_t data) { return operator=(operator unsigned() | data); }
  inline unsigned operator^=(uint8_t data) { return operator=(operator unsigned() ^ data); }
  inline unsigned operator&=(uint8_t data) { return operator=(operator unsigned() & data); }
};

struct word_t {
  union {
    uint16_t w;
#ifdef BLARGG_BIG_ENDIAN
    struct { uint8_t h, l; };
#else
    struct { uint8_t l, h; };
#endif
  };

  inline operator unsigned() const { return w; }
  inline unsigned operator=(unsigned data) { return w = data; }

  inline unsigned operator++() { return ++w; }
  inline unsigned operator--() { return --w; }

  inline unsigned operator++(int) { unsigned data = w++; return data; }
  inline unsigned operator--(int) { unsigned data = w--; return data; }

  inline unsigned operator+=(unsigned data) { return w += data;; }
  inline unsigned operator-=(unsigned data) { return w -= data;; }

  inline unsigned operator|=(unsigned data) { return w |= data; }
  inline unsigned operator^=(unsigned data) { return w ^= data; }
  inline unsigned operator&=(unsigned data) { return w &= data; }
};

struct regs_t {
  word_t pc;
  union {
    uint16_t ya;
#ifdef BLARGG_BIG_ENDIAN
    struct { uint8_t y, a; };
#else
    struct { uint8_t a, y; };
#endif
  };
  uint8_t x, s;
  flag_t p;
};
