#include "smp.hpp"

#include <cstdlib>

#define SMP_CPP
namespace SuperFamicom {

#include "memory.cpp"
#include "timing.cpp"

void SMP::step(unsigned clocks) {
  clock += clocks;
  dsp.clock -= clocks * dsp_clock_step;
}

void SMP::synchronize_dsp() {
  while(dsp.clock < 0) dsp.enter();
}

void SMP::enter() {
  while(sample_buffer < sample_buffer_end) {
    clock -= (int64_t)((double)(sample_buffer_end - sample_buffer) * 24.0 * 16.0 * tempo);
    while(status.clock_speed != 2 && clock < 0) op_step();
    if(status.clock_speed == 2) step(-clock);
    synchronize_dsp();
  }
}

void SMP::render(int16_t * buffer, unsigned count) {
  while (count > 4096) {
    sample_buffer = buffer;
    sample_buffer_end = buffer + 4096;
    buffer += 4096;
    count -= 4096;
    enter();
  }
  sample_buffer = buffer;
  sample_buffer_end = buffer + count;
  enter();
}

void SMP::skip(unsigned count) {
  while (count > 4096) {
    sample_buffer = 0;
    sample_buffer_end = ((const int16_t *)0) + 4096;
    count -= 4096;
    enter();
  }
  sample_buffer = 0;
  sample_buffer_end = ((const int16_t *)0) + count;
  enter();
}

bool SMP::sample(int16_t left, int16_t right) {
  if ( sample_buffer_end - sample_buffer < 2 ) return false;
  if ( sample_buffer > ((const int16_t *)0) + 4096 ) {
    *sample_buffer++ = left;
    *sample_buffer++ = right;
  }
  else {
    sample_buffer += 2;
  }
  return true;
}

void SMP::power() {
  //targets not initialized/changed upon reset
  timer0.target = 0;
  timer1.target = 0;
  timer2.target = 0;
    
  dsp.power();
    
  reset();
}

void SMP::reset() {
  regs.pc = 0xffc0;
  regs.a = 0x00;
  regs.x = 0x00;
  regs.y = 0x00;
  regs.s = 0xef;
  regs.p = 0x02;

  for(auto& n : apuram) n = rand();
  apuram[0x00f4] = 0x00;
  apuram[0x00f5] = 0x00;
  apuram[0x00f6] = 0x00;
  apuram[0x00f7] = 0x00;

  status.clock_counter = 0;
  status.dsp_counter = 0;
  status.timer_step = 3;

  //$00f0
  status.clock_speed = 0;
  status.timer_speed = 0;
  status.timers_enable = true;
  status.ram_disable = false;
  status.ram_writable = true;
  status.timers_disable = false;

  //$00f1
  status.iplrom_enable = true;

  //$00f2
  status.dsp_addr = 0x00;

  //$00f8,$00f9
  status.ram00f8 = 0x00;
  status.ram00f9 = 0x00;

  timer0.stage0_ticks = 0;
  timer1.stage0_ticks = 0;
  timer2.stage0_ticks = 0;

  timer0.stage1_ticks = 0;
  timer1.stage1_ticks = 0;
  timer2.stage1_ticks = 0;

  timer0.stage2_ticks = 0;
  timer1.stage2_ticks = 0;
  timer2.stage2_ticks = 0;

  timer0.stage3_ticks = 0;
  timer1.stage3_ticks = 0;
  timer2.stage3_ticks = 0;

  timer0.current_line = 0;
  timer1.current_line = 0;
  timer2.current_line = 0;

  timer0.enable = false;
  timer1.enable = false;
  timer2.enable = false;
    
  dsp.reset();
}

SMP::SMP() : dsp( *this ), timer0( *this ), timer1( *this ), timer2( *this ), clock( 0 ) {
  for(auto& byte : iplrom) byte = 0;
  set_sfm_queue(0, 0, 0);
  set_tempo(1.0);
}

SMP::~SMP() {
}

}
