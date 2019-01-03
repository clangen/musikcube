// Private oscillators used by Nes_Apu

// Nes_Snd_Emu $vers
#ifndef NES_OSCS_H
#define NES_OSCS_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Nes_Apu;

struct Nes_Osc
{
	typedef int nes_time_t;
	
	unsigned char regs [4];
	bool reg_written [4];
	Blip_Buffer* output;
	int length_counter;// length counter (0 if unused by oscillator)
	int delay;      // delay until next (potential) transition
	int last_amp;   // last amplitude oscillator was outputting
	
	void clock_length( int halt_mask );
	int period() const {
		return (regs [3] & 7) * 0x100 + (regs [2] & 0xFF);
	}
	void reset() {
		delay = 0;
		last_amp = 0;
	}
	int update_amp( int amp ) {
		int delta = amp - last_amp;
		last_amp = amp;
		return delta;
	}
};

struct Nes_Envelope : Nes_Osc
{
	int envelope;
	int env_delay;
	
	void clock_envelope();
	int volume() const;
	void reset() {
		envelope = 0;
		env_delay = 0;
		Nes_Osc::reset();
	}
};

// Nes_Square
struct Nes_Square : Nes_Envelope
{
	enum { negate_flag = 0x08 };
	enum { shift_mask = 0x07 };
	enum { phase_range = 8 };
	int phase;
	int sweep_delay;
	
	typedef Blip_Synth_Norm Synth;
	Synth const& synth; // shared between squares
	
	Nes_Square( Synth const* s ) : synth( *s ) { }
	
	void clock_sweep( int adjust );
	void run( nes_time_t, nes_time_t );
	void reset() {
		sweep_delay = 0;
		Nes_Envelope::reset();
	}
	nes_time_t maintain_phase( nes_time_t time, nes_time_t end_time,
			nes_time_t timer_period );
};

// Nes_Triangle
struct Nes_Triangle : Nes_Osc
{
	enum { phase_range = 16 };
	int phase;
	int linear_counter;
	Blip_Synth_Fast synth;
	
	int calc_amp() const;
	void run( nes_time_t, nes_time_t );
	void clock_linear_counter();
	void reset() {
		linear_counter = 0;
		phase = 1;
		Nes_Osc::reset();
	}
	nes_time_t maintain_phase( nes_time_t time, nes_time_t end_time,
			nes_time_t timer_period );
};

// Nes_Noise
struct Nes_Noise : Nes_Envelope
{
	int noise;
	Blip_Synth_Fast synth;
	
	void run( nes_time_t, nes_time_t );
	void reset() {
		noise = 1 << 14;
		Nes_Envelope::reset();
	}
};

// Nes_Dmc
struct Nes_Dmc : Nes_Osc
{
	int address;    // address of next byte to read
	int period;
	//int length_counter; // bytes remaining to play (already defined in Nes_Osc)
	int buf;
	int bits_remain;
	int bits;
	bool buf_full;
	bool silence;
	
	enum { loop_flag = 0x40 };
	
	int dac;
	
	nes_time_t next_irq;
	bool irq_enabled;
	bool irq_flag;
	bool pal_mode;
	bool nonlinear;
	
	Nes_Apu* apu;
	
	Blip_Synth_Fast synth;
	
	int  update_amp_nonlinear( int dac_in );
	void start();
	void write_register( int, int );
	void run( nes_time_t, nes_time_t );
	void recalc_irq();
	void fill_buffer();
	void reload_sample();
	void reset();
	int count_reads( nes_time_t, nes_time_t* ) const;
	nes_time_t next_read_time() const;
};

#endif
