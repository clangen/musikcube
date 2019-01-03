#ifndef PROCESSOR_SPC700_HPP
#define PROCESSOR_SPC700_HPP

#include <stdint.h>
#include <string>
#include <sstream>
#include <iomanip>
#if 1
#include <stdio.h>
#endif

namespace Processor {

struct SPC700 {
#if 0
  FILE *f;
  SPC700() { f = fopen("/tmp/spc_disasm", "w"); }
  ~SPC700() { fclose(f); }
#endif
    
  virtual void op_io() = 0;
  virtual uint8_t op_read(uint16_t addr) = 0;
  virtual void op_write(uint16_t addr, uint8_t data) = 0;
  void op_step();
    
  virtual uint8_t disassembler_read(uint16_t addr) = 0;

  #include "registers.hpp"
  #include "memory.hpp"

  regs_t regs;
  word_t dp, sp, rd, wr, bit, ya;
  uint8_t opcode;
    
  std::string disassemble_opcode(uint16_t addr);

protected:
  uint8_t op_adc(uint8_t, uint8_t);
  uint8_t op_and(uint8_t, uint8_t);
  uint8_t op_asl(uint8_t);
  uint8_t op_cmp(uint8_t, uint8_t);
  uint8_t op_dec(uint8_t);
  uint8_t op_eor(uint8_t, uint8_t);
  uint8_t op_inc(uint8_t);
  uint8_t op_ld (uint8_t, uint8_t);
  uint8_t op_lsr(uint8_t);
  uint8_t op_or (uint8_t, uint8_t);
  uint8_t op_rol(uint8_t);
  uint8_t op_ror(uint8_t);
  uint8_t op_sbc(uint8_t, uint8_t);
  uint8_t op_st (uint8_t, uint8_t);
  uint16_t op_adw(uint16_t, uint16_t);
  uint16_t op_cpw(uint16_t, uint16_t);
  uint16_t op_ldw(uint16_t, uint16_t);
  uint16_t op_sbw(uint16_t, uint16_t);

  template<uint8_t (SPC700::*op)(uint8_t)> void op_adjust(uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t)> void op_adjust_addr();
  template<uint8_t (SPC700::*op)(uint8_t)> void op_adjust_dp();
  void op_adjust_dpw(signed);
  template<uint8_t (SPC700::*op)(uint8_t)> void op_adjust_dpx();
  void op_branch(bool);
  void op_branch_bit();
  void op_pull(uint8_t&);
  void op_push(uint8_t);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_addr(uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_addri(uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_const(uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_dp(uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_dpi(uint8_t&, uint8_t&);
  template<uint16_t (SPC700::*op)(uint16_t, uint16_t)> void op_read_dpw();
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_idpx();
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_idpy();
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_read_ix();
  void op_set_addr_bit();
  void op_set_bit();
  void op_set_flag(bool&, bool);
  void op_test_addr(bool);
  void op_transfer(uint8_t&, uint8_t&);
  void op_write_addr(uint8_t&);
  void op_write_addri(uint8_t&);
  void op_write_dp(uint8_t&);
  void op_write_dpi(uint8_t&, uint8_t&);
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_write_dp_const();
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_write_dp_dp();
  template<uint8_t (SPC700::*op)(uint8_t, uint8_t)> void op_write_ix_iy();

  void op_bne_dp();
  void op_bne_dpdec();
  void op_bne_dpx();
  void op_bne_ydec();
  void op_brk();
  void op_clv();
  void op_cmc();
  void op_daa();
  void op_das();
  void op_div_ya_x();
  void op_jmp_addr();
  void op_jmp_iaddrx();
  void op_jsp_dp();
  void op_jsr_addr();
  void op_jst();
  void op_lda_ixinc();
  void op_mul_ya();
  void op_nop();
  void op_plp();
  void op_rti();
  void op_rts();
  void op_sta_idpx();
  void op_sta_idpy();
  void op_sta_ix();
  void op_sta_ixinc();
  void op_stw_dp();
  void op_wait();
  void op_xcn();
};

}

#endif
