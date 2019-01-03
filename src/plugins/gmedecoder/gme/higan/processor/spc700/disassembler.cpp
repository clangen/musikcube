template<signed precision> std::string hex(uintmax_t value) {
  std::ostringstream s;
  s << std::hex << std::setw( precision ) << std::setfill( '0' ) << value;
  return s.str();
}

std::string SPC700::disassemble_opcode(uint16_t addr) {
  auto read = [&](uint16_t addr) -> uint8_t {
    return disassembler_read(addr);
  };	

  /*auto relative = [&](unsigned length, int8_t offset) -> uint16_t {
    uint16_t pc = addr + length;
    return pc + offset;
  };*/

  auto a = [&]{ return hex<4>((read(addr + 1) << 0) + (read(addr + 2) << 8)); };
  auto b = [&](unsigned n) { return hex<2>(read(addr + 1 + n)); };
  auto r = [&](unsigned r) { return hex<4>(addr + r + (int8_t)read(addr + 1)); };
  auto ro = [&](unsigned r, unsigned n) { return hex<4>(addr + r + (int8_t)read(addr + 1 + n)); };
  auto dp = [&](unsigned n) { return hex<3>((regs.p.p << 8) + read(addr + 1 + n)); };
  auto ab = [&] {
    unsigned n = (read(addr + 1) << 0) + (read(addr + 2) << 8);
    return hex<4>(n & 0x1fff) + ":" + hex<1>(n >> 13);
  };

  auto mnemonic = [&]() -> std::string {
    switch(read(addr)) {
    case 0x00: return "nop";
    case 0x01: return "jst $ffde";
    case 0x02: return (std::string)"set $" + dp(0) + ":0";
    case 0x03: return (std::string)"bbs $" + dp(0) + ":0=$" + ro(+3, 1);
    case 0x04: return (std::string)"ora $" + dp(0);
    case 0x05: return (std::string)"ora $" + a();
    case 0x06: return "ora (x)";
    case 0x07: return (std::string)"ora ($" + dp(0) + ",x)";
    case 0x08: return (std::string)"ora #$" + b(0);
    case 0x09: return (std::string)"orr $" + dp(1) + "=$" + dp(0);
    case 0x0a: return (std::string)"orc $" + ab();
    case 0x0b: return (std::string)"asl $" + dp(0);
    case 0x0c: return (std::string)"asl $" + a();
    case 0x0d: return "php";
    case 0x0e: return (std::string)"tsb $" + a();
    case 0x0f: return "brk";
    case 0x10: return (std::string)"bpl $" + r(+2);
    case 0x11: return "jst $ffdc";
    case 0x12: return (std::string)"clr $" + dp(0) + ":0";
    case 0x13: return (std::string)"bbc $" + dp(0) + ":0=$" + ro(+3, 1);
    case 0x14: return (std::string)"ora $" + dp(0) + ",x";
    case 0x15: return (std::string)"ora $" + a() + ",x";
    case 0x16: return (std::string)"ora $" + a() + ",y";
    case 0x17: return (std::string)"ora ($" + dp(0) + "),y";
    case 0x18: return (std::string)"orr $" + dp(1) + "=#$" + b(0);
    case 0x19: return "orr (x)=(y)";
    case 0x1a: return (std::string)"dew $" + dp(0);
    case 0x1b: return (std::string)"asl $" + dp(0) + ",x";
    case 0x1c: return "asl";
    case 0x1d: return "dex";
    case 0x1e: return (std::string)"cpx $" + a();
    case 0x1f: return (std::string)"jmp ($" + a() + ",x)";
    case 0x20: return "clp";
    case 0x21: return "jst $ffda";
    case 0x22: return (std::string)"set $" + dp(0) + ":1";
    case 0x23: return (std::string)"bbs $" + dp(0) + ":1=$" + ro(+3, 1);
    case 0x24: return (std::string)"and $" + dp(0);
    case 0x25: return (std::string)"and $" + a();
    case 0x26: return "and (x)";
    case 0x27: return (std::string)"and ($" + dp(0) + ",x)";
    case 0x29: return (std::string)"and $" + dp(1) + "=$" + dp(0);
    case 0x2a: return (std::string)"orc !$" + ab();
    case 0x2b: return (std::string)"rol $" + dp(0);
    case 0x2c: return (std::string)"rol $" + a();
    case 0x2d: return "pha";
    case 0x2e: return (std::string)"bne $" + dp(0) + "=$" + ro(+3, 1);
    case 0x28: return (std::string)"and #$" + b(0);
    case 0x2f: return (std::string)"bra $" + r(+2);
    case 0x30: return (std::string)"bmi $" + r(+2);
    case 0x31: return "jst $ffd8";
    case 0x32: return (std::string)"clr $" + dp(0) + ":1";
    case 0x33: return (std::string)"bbc $" + dp(0) + ":1=$" + ro(+3, 1);
    case 0x34: return (std::string)"and $" + dp(0) + ",x";
    case 0x35: return (std::string)"and $" + a() + ",x";
    case 0x36: return (std::string)"and $" + a() + ",y";
    case 0x37: return (std::string)"and ($" + dp(0) + "),y";
    case 0x38: return (std::string)"and $" + dp(1) + "=#$" + b(0);
    case 0x39: return "and (x)=(y)";
    case 0x3a: return (std::string)"inw $" + dp(0);
    case 0x3b: return (std::string)"rol $" + dp(0) + ",x";
    case 0x3c: return "rol";
    case 0x3d: return "inx";
    case 0x3e: return (std::string)"cpx $" + dp(0);
    case 0x3f: return (std::string)"jsr $" + a();
    case 0x40: return "sep";
    case 0x41: return "jst $ffd6";
    case 0x42: return (std::string)"set $" + dp(0) + ":2";
    case 0x43: return (std::string)"bbs $" + dp(0) + ":2=$" + ro(+3, 1);
    case 0x44: return (std::string)"eor $" + dp(0);
    case 0x45: return (std::string)"eor $" + a();
    case 0x46: return "eor (x)";
    case 0x47: return (std::string)"eor ($" + dp(0) + ",x)";
    case 0x48: return (std::string)"eor #$" + b(0);
    case 0x49: return (std::string)"eor $" + dp(1) + "=$" + dp(0);
    case 0x4a: return (std::string)"and $" + ab();
    case 0x4b: return (std::string)"lsr $" + dp(0);
    case 0x4c: return (std::string)"lsr $" + a();
    case 0x4d: return "phx";
    case 0x4e: return (std::string)"trb $" + a();
    case 0x4f: return (std::string)"jsp $ff" + b(0);
    case 0x50: return (std::string)"bvc $" + r(+2);
    case 0x51: return "jst $ffd4";
    case 0x52: return (std::string)"clr $" + dp(0) + ":2";
    case 0x53: return (std::string)"bbc $" + dp(0) + ":2=$" + ro(+3, 1);
    case 0x54: return (std::string)"eor $" + dp(0) + ",x";
    case 0x55: return (std::string)"eor $" + a() + ",x";
    case 0x56: return (std::string)"eor $" + a() + ",y";
    case 0x57: return (std::string)"eor ($" + dp(0) + "),y";
    case 0x58: return (std::string)"eor $" + dp(1) + "=#$" + b(0);
    case 0x59: return "eor (x)=(y)";
    case 0x5a: return (std::string)"cpw $" + a();
    case 0x5b: return (std::string)"lsr $" + dp(0) + ",x";
    case 0x5c: return "lsr";
    case 0x5d: return "tax";
    case 0x5e: return (std::string)"cpy $" + a();
    case 0x5f: return (std::string)"jmp $" + a();
    case 0x60: return "clc";
    case 0x61: return "jst $ffd2";
    case 0x62: return (std::string)"set $" + dp(0) + ":3";
    case 0x63: return (std::string)"bbs $" + dp(0) + ":3=$" + ro(+3, 1);
    case 0x64: return (std::string)"cmp $" + dp(0);
    case 0x65: return (std::string)"cmp $" + a();
    case 0x66: return "cmp (x)";
    case 0x67: return (std::string)"cmp ($" + dp(0) + ",x)";
    case 0x68: return (std::string)"cmp #$" + b(0);
    case 0x69: return (std::string)"cmp $" + dp(1) + "=$" + dp(0);
    case 0x6a: return (std::string)"and !$" + ab();
    case 0x6b: return (std::string)"ror $" + dp(0);
    case 0x6c: return (std::string)"ror $" + a();
    case 0x6d: return "phy";
    case 0x6e: return (std::string)"bne --$" + dp(0) + "=$" + ro(+3, 1);
    case 0x6f: return "rts";
    case 0x70: return (std::string)"bvs $" + r(+2);
    case 0x71: return "jst $ffd0";
    case 0x72: return (std::string)"clr $" + dp(0) + ":3";
    case 0x73: return (std::string)"bbc $" + dp(0) + ":3=$" + ro(+3, 1);
    case 0x74: return (std::string)"cmp $" + dp(0) + ",x";
    case 0x75: return (std::string)"cmp $" + a() + ",x";
    case 0x76: return (std::string)"cmp $" + a() + ",y";
    case 0x77: return (std::string)"cmp ($" + dp(0) + "),y";
    case 0x78: return (std::string)"cmp $" + dp(1) + "=#$" + b(0);
    case 0x79: return "cmp (x)=(y)";
    case 0x7a: return (std::string)"adw $" + a();
    case 0x7b: return (std::string)"ror $" + dp(0) + ",x";
    case 0x7c: return "ror";
    case 0x7d: return "txa";
    case 0x7e: return (std::string)"cpy $" + dp(0);
    case 0x7f: return "rti";
    case 0x80: return "sec";
    case 0x81: return "jst $ffce";
    case 0x82: return (std::string)"set $" + dp(0) + ":4";
    case 0x83: return (std::string)"bbs $" + dp(0) + ":4=$" + ro(+3, 1);
    case 0x84: return (std::string)"adc $" + dp(0);
    case 0x85: return (std::string)"adc $" + a();
    case 0x86: return "adc (x)";
    case 0x87: return (std::string)"adc ($" + dp(0) + ",x)";
    case 0x88: return (std::string)"adc #$" + b(0);
    case 0x89: return (std::string)"adc $" + dp(1) + "=$" + dp(0);
    case 0x8a: return (std::string)"eor $" + ab();
    case 0x8b: return (std::string)"dec $" + dp(0);
    case 0x8c: return (std::string)"dec $" + a();
    case 0x8d: return (std::string)"ldy #$" + b(0);
    case 0x8e: return "plp";
    case 0x8f: return (std::string)"str $" + dp(1) + "=#$" + b(0);
    case 0x90: return (std::string)"bcc $" + r(+2);
    case 0x91: return "jst $ffcc";
    case 0x92: return (std::string)"clr $" + dp(0) + ":4";
    case 0x93: return (std::string)"bbc $" + dp(0) + ":4=$" + ro(+3, 1);
    case 0x94: return (std::string)"adc $" + dp(0) + ",x";
    case 0x95: return (std::string)"adc $" + a() + ",x";
    case 0x96: return (std::string)"adc $" + a() + ",y";
    case 0x97: return (std::string)"adc ($" +dp(0) + "),y";
    case 0x98: return (std::string)"adc $" + dp(1) + "=#$" + b(0);
    case 0x99: return "adc (x)=(y)";
    case 0x9a: return (std::string)"sbw $" + a();
    case 0x9b: return (std::string)"dec $" + dp(0) + ",x";
    case 0x9c: return "dec";
    case 0x9d: return "tsx";
    case 0x9e: return "div";
    case 0x9f: return "xcn";
    case 0xa0: return "sei";
    case 0xa1: return "jst $ffca";
    case 0xa2: return (std::string)"set $" + dp(0) + ":5";
    case 0xa3: return (std::string)"bbs $" + dp(0) + ":5=$" + ro(+3, 1);
    case 0xa4: return (std::string)"sbc $" + dp(0);
    case 0xa5: return (std::string)"sbc $" + a();
    case 0xa6: return "sbc (x)";
    case 0xa7: return (std::string)"sbc ($" + dp(0) + ",x)";
    case 0xa8: return (std::string)"sbc #$" + b(0);
    case 0xa9: return (std::string)"sbc $" + dp(1) + "=$" + dp(0);
    case 0xaa: return (std::string)"ldc $" + ab();
    case 0xab: return (std::string)"inc $" + dp(0);
    case 0xac: return (std::string)"inc $" + a();
    case 0xad: return (std::string)"cpy #$" + b(0);
    case 0xae: return "pla";
    case 0xaf: return "sta (x++)";
    case 0xb0: return (std::string)"bcs $" + r(+2);
    case 0xb1: return "jst $ffc8";
    case 0xb2: return (std::string)"clr $" + dp(0) + ":5";
    case 0xb3: return (std::string)"bbc $" + dp(0) + ":5=$" + ro(+3, 1);
    case 0xb4: return (std::string)"sbc $" + dp(0) + ",x";
    case 0xb5: return (std::string)"sbc $" + a() + ",x";
    case 0xb6: return (std::string)"sbc $" + a() + ",y";
    case 0xb7: return (std::string)"sbc ($" + dp(0) + "),y";
    case 0xb8: return (std::string)"sbc $" + dp(1) + "=#$" + b(0);
    case 0xb9: return "sbc (x)=(y)";
    case 0xba: return (std::string)"ldw $" + dp(0);
    case 0xbb: return (std::string)"inc $" + dp(0) + ",x";
    case 0xbc: return "inc";
    case 0xbd: return "txs";
    case 0xbe: return "das";
    case 0xbf: return "lda (x++)";
    case 0xc0: return "cli";
    case 0xc1: return "jst $ffc6";
    case 0xc2: return (std::string)"set $" + dp(0) + ":6";
    case 0xc3: return (std::string)"bbs $" + dp(0) + ":6=$" + ro(+3, 1);
    case 0xc4: return (std::string)"sta $" + dp(0);
    case 0xc5: return (std::string)"sta $" + a();
    case 0xc6: return "sta (x)";
    case 0xc7: return (std::string)"sta ($" + dp(0) + ",x)";
    case 0xc8: return (std::string)"cpx #$" + b(0);
    case 0xc9: return (std::string)"stx $" + a();
    case 0xca: return (std::string)"stc $" + ab();
    case 0xcb: return (std::string)"sty $" + dp(0);
    case 0xcc: return (std::string)"sty $" + a();
    case 0xcd: return (std::string)"ldx #$" + b(0);
    case 0xce: return "plx";
    case 0xcf: return "mul";
    case 0xd0: return (std::string)"bne $" + r(+2);
    case 0xd1: return "jst $ffc4";
    case 0xd2: return (std::string)"clr $" + dp(0) + ":6";
    case 0xd3: return (std::string)"bbc $" + dp(0) + ":6=$" + ro(+3, 1);
    case 0xd4: return (std::string)"sta $" + dp(0) + ",x";
    case 0xd5: return (std::string)"sta $" + a() + ",x";
    case 0xd6: return (std::string)"sta $" + a() + ",y";
    case 0xd7: return (std::string)"sta ($" + dp(0) + "),y";
    case 0xd8: return (std::string)"stx $" + dp(0);
    case 0xd9: return (std::string)"stx $" + dp(0) + ",y";
    case 0xda: return (std::string)"stw $" + dp(0);
    case 0xdb: return (std::string)"sty $" + dp(0) + ",x";
    case 0xdc: return "dey";
    case 0xdd: return "tya";
    case 0xde: return (std::string)"bne $" + dp(0) + ",x=$" + ro(+3, 1);
    case 0xdf: return "daa";
    case 0xe0: return "clv";
    case 0xe1: return "jst $ffc2";
    case 0xe2: return (std::string)"set $" + dp(0) + ":7";
    case 0xe3: return (std::string)"bbs $" + dp(0) + ":7=$" + ro(+3, 1);
    case 0xe4: return (std::string)"lda $" + dp(0);
    case 0xe5: return (std::string)"lda $" + a();
    case 0xe6: return "lda (x)";
    case 0xe7: return (std::string)"lda ($" + dp(0) + ",x)";
    case 0xe8: return (std::string)"lda #$" + b(0);
    case 0xe9: return (std::string)"ldx $" + a();
    case 0xea: return (std::string)"not $" + ab();
    case 0xeb: return (std::string)"ldy $" + dp(0);
    case 0xec: return (std::string)"ldy $" + a();
    case 0xed: return "cmc";
    case 0xee: return "ply";
    case 0xef: return "wai";
    case 0xf0: return (std::string)"beq $" + r(+2);
    case 0xf1: return "jst $ffc0";
    case 0xf2: return (std::string)"clr $" + dp(0) + ":7";
    case 0xf3: return (std::string)"bbc $" + dp(0) + ":7=$" + ro(+3, 1);
    case 0xf4: return (std::string)"lda $" + dp(0) + ",x";
    case 0xf5: return (std::string)"lda $" + a() + ",x";
    case 0xf6: return (std::string)"lda $" + a() + ",y";
    case 0xf7: return (std::string)"lda ($" + dp(0) + "),y";
    case 0xf8: return (std::string)"ldx $" + dp(0);
    case 0xf9: return (std::string)"ldx $" + dp(0) + ",y";
    case 0xfa: return (std::string)"str $" + dp(1) + "=$" + dp(0);
    case 0xfb: return (std::string)"ldy $" + dp(0) + ",x";
    case 0xfc: return "iny";
    case 0xfd: return "tay";
    case 0xfe: return (std::string)"bne --y=$" + r(+2);
    case 0xff: return "stp";
    }
    throw;
  };

  std::string output = (std::string)".." + hex<4>(addr) + " " + mnemonic();

  unsigned length = output.length();
  while(length++ < 30) output.append(" ");

  output +=
    "YA:" + hex<4>(regs.ya) +
    " A:" + hex<2>(regs.a) +
    " X:" + hex<2>(regs.x) +
    " Y:" + hex<2>(regs.y) +
    " S:" + hex<2>(regs.s) +
    " " +
    (regs.p.n ? "N" : "n") +
    (regs.p.v ? "V" : "v") +
    (regs.p.p ? "P" : "p") +
    (regs.p.b ? "B" : "b") +
    (regs.p.h ? "H" : "h") +
    (regs.p.i ? "I" : "i") +
    (regs.p.z ? "Z" : "z") +
    (regs.p.c ? "C" : "c");

  return output;
}
