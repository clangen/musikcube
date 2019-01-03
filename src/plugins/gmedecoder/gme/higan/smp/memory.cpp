#ifdef SMP_CPP

inline uint8_t SMP::ram_read(uint16_t addr) {
  if(addr >= 0xffc0 && status.iplrom_enable) return iplrom[addr & 0x3f];
  if(status.ram_disable) return 0x5a;  //0xff on mini-SNES
  return apuram[addr];
}

inline void SMP::ram_write(uint16_t addr, uint8_t data) {
  //writes to $ffc0-$ffff always go to apuram, even if the iplrom is enabled
  if(status.ram_writable && !status.ram_disable) apuram[addr] = data;
}

uint8_t SMP::port_read(uint8_t port) const {
  return apuram[0xf4 + (port & 3)];
}

void SMP::port_write(uint8_t port, uint8_t data) {
  apuram[0xf4 + (port & 3)] = data;
}

uint8_t SMP::op_busread(uint16_t addr) {
  unsigned result;

  switch(addr) {
  case 0xf0:  //TEST -- write-only register
    return 0x00;

  case 0xf1:  //CONTROL -- write-only register
    return 0x00;

  case 0xf2:  //DSPADDR
    return status.dsp_addr;

  case 0xf3:  //DSPDATA
    //0x80-0xff are read-only mirrors of 0x00-0x7f
    return dsp.read(status.dsp_addr & 0x7f);

  case 0xf4:  //CPUIO0
  case 0xf5:  //CPUIO1
  case 0xf6:  //CPUIO2
  case 0xf7:  //CPUIO3
    if (sfm_queue && sfm_queue < sfm_queue_end) {
      result = *sfm_queue;
      if (++sfm_queue == sfm_queue_end)
        sfm_queue = sfm_queue_repeat;
      sfm_last[addr - 0xf4] = result;
      return result;
    }
    return sfm_last[addr - 0xf4];

  case 0xf8:  //RAM0
    return status.ram00f8;

  case 0xf9:  //RAM1
    return status.ram00f9;

  case 0xfa:  //T0TARGET
  case 0xfb:  //T1TARGET
  case 0xfc:  //T2TARGET -- write-only registers
    return 0x00;

  case 0xfd:  //T0OUT -- 4-bit counter value
    result = timer0.stage3_ticks;
    timer0.stage3_ticks = 0;
    return result;

  case 0xfe:  //T1OUT -- 4-bit counter value
    result = timer1.stage3_ticks;
    timer1.stage3_ticks = 0;
    return result;

  case 0xff:  //T2OUT -- 4-bit counter value
    result = timer2.stage3_ticks;
    timer2.stage3_ticks = 0;
    return result;
  }

  return ram_read(addr);
}

void SMP::op_buswrite(uint16_t addr, uint8_t data) {
  switch(addr) {
  case 0xf0:  //TEST
    if(regs.p.p) break;  //writes only valid when P flag is clear

    status.clock_speed    = (data >> 6) & 3;
    status.timer_speed    = (data >> 4) & 3;
    status.timers_enable  = data & 0x08;
    status.ram_disable    = data & 0x04;
    status.ram_writable   = data & 0x02;
    status.timers_disable = data & 0x01;

    status.timer_step = (1 << status.clock_speed) + (2 << status.timer_speed);

    timer0.synchronize_stage1();
    timer1.synchronize_stage1();
    timer2.synchronize_stage1();
    break;

  case 0xf1:  //CONTROL
    status.iplrom_enable = data & 0x80;
          
    if (data & 0x10) {
      sfm_last[ 0 ] = 0;
      sfm_last[ 1 ] = 0;
    }
    if (data & 0x20) {
      sfm_last[ 2 ] = 0;
      sfm_last[ 3 ] = 0;
    }

    //0->1 transistion resets timers
    if(timer2.enable == false && (data & 0x04)) {
      timer2.stage2_ticks = 0;
      timer2.stage3_ticks = 0;
    }
    timer2.enable = data & 0x04;

    if(timer1.enable == false && (data & 0x02)) {
      timer1.stage2_ticks = 0;
      timer1.stage3_ticks = 0;
    }
    timer1.enable = data & 0x02;

    if(timer0.enable == false && (data & 0x01)) {
      timer0.stage2_ticks = 0;
      timer0.stage3_ticks = 0;
    }
    timer0.enable = data & 0x01;
    break;

  case 0xf2:  //DSPADDR
    status.dsp_addr = data;
    break;

  case 0xf3:  //DSPDATA
    if(status.dsp_addr & 0x80) break;  //0x80-0xff are read-only mirrors of 0x00-0x7f
    dsp.write(status.dsp_addr & 0x7f, data);
    break;

  case 0xf4:  //CPUIO0
  case 0xf5:  //CPUIO1
  case 0xf6:  //CPUIO2
  case 0xf7:  //CPUIO3
    port_write(addr, data);
    break;

  case 0xf8:  //RAM0
    status.ram00f8 = data;
    break;

  case 0xf9:  //RAM1
    status.ram00f9 = data;
    break;

  case 0xfa:  //T0TARGET
    timer0.target = data;
    break;

  case 0xfb:  //T1TARGET
    timer1.target = data;
    break;

  case 0xfc:  //T2TARGET
    timer2.target = data;
    break;

  case 0xfd:  //T0OUT
  case 0xfe:  //T1OUT
  case 0xff:  //T2OUT -- read-only registers
    break;
  }

  ram_write(addr, data);  //all writes, even to MMIO registers, appear on bus
}

void SMP::op_io() {
  add_clocks(24);
  cycle_edge();
}

uint8_t SMP::op_read(uint16_t addr) {
  add_clocks(12);
  uint8_t r = op_busread(addr);
  add_clocks(12);
  cycle_edge();
  return r;
}

void SMP::op_write(uint16_t addr, uint8_t data) {
  add_clocks(24);
  op_buswrite(addr, data);
  cycle_edge();
}

uint8_t SMP::disassembler_read(uint16_t addr) {
    if((addr & 0xfff0) == 0x00f0) return 0x00;
    if((addr & 0xffc0) == 0xffc0 && status.iplrom_enable) return iplrom[addr & 0x3f];
    return apuram[addr];
}

#endif
