inline uint8_t op_readpc() {
  return op_read(regs.pc++);
}

inline uint8_t op_readsp() {
  return op_read(0x0100 | ++regs.s);
}

inline void op_writesp(uint8_t data) {
  return op_write(0x0100 | regs.s--, data);
}

inline uint8_t op_readdp(uint8_t addr) {
  return op_read((regs.p.p << 8) + addr);
}

inline void op_writedp(uint8_t addr, uint8_t data) {
  return op_write((regs.p.p << 8) + addr, data);
}
