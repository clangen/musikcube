#ifndef _higan_dsp_h_
#define _higan_dsp_h_

#include "SPC_DSP.h"

#include "blargg_common.h"

namespace SuperFamicom {

struct DSP {
  int64_t clock;
  unsigned long removed_samples;

  inline void step(uint64_t clocks);

  bool mute();
  uint8_t read(uint8_t addr);
  void write(uint8_t addr, uint8_t data);

  void enter();
  void power();
  void reset();

  void channel_enable(unsigned channel, bool enable);
  void disable_surround(bool disable = true);

  DSP(struct SMP&);
  ~DSP();

  SPC_DSP spc_dsp;

private:
  struct SMP & smp;
  int16_t * samplebuffer;
  bool channel_enabled[8];
};

};

#endif
