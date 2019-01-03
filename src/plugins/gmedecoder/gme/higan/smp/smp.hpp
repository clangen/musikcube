#ifndef _higan_smp_h_
#define _higan_smp_h_

#include "blargg_common.h"

#include "../processor/spc700/spc700.hpp"

#include "../dsp/dsp.hpp"

namespace SuperFamicom {

struct SMP : Processor::SPC700 {
  long clock;

  uint8_t iplrom[64];
  uint8_t apuram[64 * 1024];

  double tempo;
  int64_t dsp_clock_step;
  SuperFamicom::DSP dsp;

  inline void step(unsigned clocks);
  inline void synchronize_dsp();

  uint8_t port_read(uint8_t port) const;
  void port_write(uint8_t port, uint8_t data);

  void enter();
  void power();
  void reset();

  void set_tempo(double);

  void render(int16_t * buffer, unsigned count);
  void skip(unsigned count);

  uint8_t sfm_last[4];
private:
  uint8_t const* sfm_queue;
  uint8_t const* sfm_queue_end;
  uint8_t const* sfm_queue_repeat;
public:
  void set_sfm_queue(const uint8_t* queue, const uint8_t* queue_end, const uint8_t* queue_repeat);

  const uint8_t* get_sfm_queue() const;
  size_t get_sfm_queue_remain() const;

private:
  int16_t * sample_buffer;
  int16_t const* sample_buffer_end;
public:
  bool sample( int16_t, int16_t );

  SMP();
  ~SMP();

  struct {
    //timing
    unsigned clock_counter;
    unsigned dsp_counter;
    unsigned timer_step;

    //$00f0
    uint8_t clock_speed;
    uint8_t timer_speed;
    bool timers_enable;
    bool ram_disable;
    bool ram_writable;
    bool timers_disable;

    //$00f1
    bool iplrom_enable;

    //$00f2
    uint8_t dsp_addr;

    //$00f8,$00f9
    uint8_t ram00f8;
    uint8_t ram00f9;
  } status;

  friend class SMPcore;

  //memory.cpp
  uint8_t ram_read(uint16_t addr);
  void ram_write(uint16_t addr, uint8_t data);

  uint8_t op_busread(uint16_t addr);
  void op_buswrite(uint16_t addr, uint8_t data);

  void op_io();
  uint8_t op_read(uint16_t addr);
  void op_write(uint16_t addr, uint8_t data);

  uint8_t disassembler_read(uint16_t addr);

  //timing.cpp
  template<unsigned frequency>
  struct Timer {
    SMP &smp;
    uint8_t stage0_ticks;
    uint8_t stage1_ticks;
    uint8_t stage2_ticks;
    uint8_t stage3_ticks;
    bool current_line;
    bool enable;
    uint8_t target;

    Timer(SMP &p_smp) : smp( p_smp ) { }

    void tick();
    void synchronize_stage1();
  };

  Timer<192> timer0;
  Timer<192> timer1;
  Timer< 24> timer2;

  inline void add_clocks(unsigned clocks);
  inline void cycle_edge();
};

inline void SMP::set_tempo(double speed) { tempo = speed; dsp_clock_step = (int64_t)(4096.0 / speed); }

inline void SMP::set_sfm_queue(const uint8_t *queue, const uint8_t *queue_end, const uint8_t *queue_repeat) { sfm_queue = queue; sfm_queue_end = queue_end; sfm_queue_repeat = queue_repeat; sfm_last[0] = 0; sfm_last[1] = 0; sfm_last[2] = 0; sfm_last[3] = 0; }

inline const uint8_t* SMP::get_sfm_queue() const { return sfm_queue; }
inline size_t SMP::get_sfm_queue_remain() const { return sfm_queue_end - sfm_queue; }

};

#endif
