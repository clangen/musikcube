#include "../smp/smp.hpp"
#include "dsp.hpp"

namespace SuperFamicom {

void DSP::step(uint64_t clocks) {
  clock += clocks;
}

void DSP::enter() {
  int64_t dsp_clocks = (-clock) / (24 * 4096) + 1;
  if (dsp_clocks < 1) return;

  spc_dsp.run(dsp_clocks);
  step(dsp_clocks * 24 * 4096);

  samplebuffer = spc_dsp.get_output();
  signed count = spc_dsp.sample_count();
  if(count > removed_samples) {
    for(unsigned n = removed_samples; n < count; n += 2) {
      if (!smp.sample(samplebuffer[n + 0], samplebuffer[n + 1])) {
        removed_samples = n;
        return;
      }
    }
    spc_dsp.set_output(samplebuffer, 8192);
    removed_samples = 0;
  }
}

bool DSP::mute() {
  return spc_dsp.mute();
}

uint8_t DSP::read(uint8_t addr) {
  return spc_dsp.read(addr);
}

void DSP::write(uint8_t addr, uint8_t data) {
  spc_dsp.write(addr, data);
}

void DSP::power() {
  spc_dsp.init(smp.apuram);
  spc_dsp.reset();
  spc_dsp.set_output(0, 0);
  removed_samples = 0;
}

void DSP::reset() {
  spc_dsp.soft_reset();
  spc_dsp.set_output(0, 0);
  removed_samples = 0;
}

void DSP::channel_enable(unsigned channel, bool enable) {
  channel_enabled[channel & 7] = enable;
  unsigned mask = 0;
  for(unsigned i = 0; i < 8; i++) {
    if(channel_enabled[i] == false) mask |= 1 << i;
  }
  spc_dsp.mute_voices(mask);
}
    
void DSP::disable_surround(bool disable) {
  spc_dsp.disable_surround(disable);
}

DSP::DSP(struct SMP & p_smp)
    : smp( p_smp ), clock( 0 ), removed_samples( 0 ), samplebuffer( 0 ) {
  for(unsigned i = 0; i < 8; i++) channel_enabled[i] = true;
}

DSP::~DSP() {
  if ( samplebuffer ) free( samplebuffer );
}

}
