// YM2610B FM sound chip emulator interface

// Game_Music_Emu $vers
#ifndef YM2610B_EMU_H
#define YM2610B_EMU_H

#include "Ay_Apu.h"

class Ym2610b_Emu  {
	void* opn;
	Ay_Apu psg;
	Blip_Buffer buffer;
	unsigned sample_rate;
	unsigned psg_clock;
	bool is_2610b;
public:
	Ym2610b_Emu();
	~Ym2610b_Emu();
	
	// Sets output chip clock rate, in Hz. Returns non-zero
	// if error.
	int set_rate( int sample_rate, int clock_rate, bool is_2610b );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 9 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write0( int addr, int data );
	void write1( int addr, int data );
	
	// Sets ROM type, scales ROM size, then writes length bytes from data at start offset
	void write_rom( int rom_id, int size, int start, int length, void * data );

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
