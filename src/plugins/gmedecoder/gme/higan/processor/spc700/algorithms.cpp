uint8_t SPC700::op_adc(uint8_t x, uint8_t y) {
  int r = x + y + regs.p.c;
  regs.p.n = r & 0x80;
  regs.p.v = ~(x ^ y) & (x ^ r) & 0x80;
  regs.p.h = (x ^ y ^ r) & 0x10;
  regs.p.z = (uint8_t)r == 0;
  regs.p.c = r > 0xff;
  return r;
}

uint8_t SPC700::op_and(uint8_t x, uint8_t y) {
  x &= y;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_asl(uint8_t x) {
  regs.p.c = x & 0x80;
  x <<= 1;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_cmp(uint8_t x, uint8_t y) {
  int r = x - y;
  regs.p.n = r & 0x80;
  regs.p.z = (uint8_t)r == 0;
  regs.p.c = r >= 0;
  return x;
}

uint8_t SPC700::op_dec(uint8_t x) {
  x--;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_eor(uint8_t x, uint8_t y) {
  x ^= y;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_inc(uint8_t x) {
  x++;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_ld(uint8_t x, uint8_t y) {
  regs.p.n = y & 0x80;
  regs.p.z = y == 0;
  return y;
}

uint8_t SPC700::op_lsr(uint8_t x) {
  regs.p.c = x & 0x01;
  x >>= 1;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_or(uint8_t x, uint8_t y) {
  x |= y;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_rol(uint8_t x) {
  unsigned carry = regs.p.c << 0;
  regs.p.c = x & 0x80;
  x = (x << 1) | carry;
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_ror(uint8_t x) {
  unsigned carry = regs.p.c << 7;
  regs.p.c = x & 0x01;
  x = carry | (x >> 1);
  regs.p.n = x & 0x80;
  regs.p.z = x == 0;
  return x;
}

uint8_t SPC700::op_sbc(uint8_t x, uint8_t y) {
  return op_adc(x, ~y);
}

uint8_t SPC700::op_st(uint8_t x, uint8_t y) {
  return y;
}

//

uint16_t SPC700::op_adw(uint16_t x, uint16_t y) {
  uint16_t r;
  regs.p.c = 0;
  r  = op_adc(x, y);
  r |= op_adc(x >> 8, y >> 8) << 8;
  regs.p.z = r == 0;
  return r;
}

uint16_t SPC700::op_cpw(uint16_t x, uint16_t y) {
  int r = x - y;
  regs.p.n = r & 0x8000;
  regs.p.z = (uint16_t)r == 0;
  regs.p.c = r >= 0;
  return x;
}

uint16_t SPC700::op_ldw(uint16_t x, uint16_t y) {
  regs.p.n = y & 0x8000;
  regs.p.z = y == 0;
  return y;
}

uint16_t SPC700::op_sbw(uint16_t x, uint16_t y) {
  uint16_t r;
  regs.p.c = 1;
  r  = op_sbc(x, y);
  r |= op_sbc(x >> 8, y >> 8) << 8;
  regs.p.z = r == 0;
  return r;
}
