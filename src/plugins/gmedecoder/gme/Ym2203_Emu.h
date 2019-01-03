// YM2203 FM sound chip emulator interface

// Game_Music_Emu $vers
#ifndef YM2203_EMU_H
#define YM2203_EMU_H

#include "Ay_Apu.h"

class Ym2203_Emu  {
	void* opn;
	Ay_Apu psg;
	Blip_Buffer buffer;
	unsigned sample_rate;
	unsigned psg_clock;
public:
	Ym2203_Emu();
	~Ym2203_Emu();
	
	// Sets output chip clock rate, in Hz. Returns non-zero
	// if error.
	int set_rate( int sample_rate, int clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 6 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write( int addr, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );

	// SSG interface
	inline void psg_set_clock( int clock );
	inline void psg_write( int addr, int data );
	inline int psg_read();
	inline void psg_reset();
};

#endif
