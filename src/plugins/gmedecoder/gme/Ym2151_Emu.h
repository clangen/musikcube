// YM2151 FM sound chip emulator interface

// Game_Music_Emu $vers
#ifndef YM2151_EMU_H
#define YM2151_EMU_H

class Ym2151_Emu  {
	void* PSG;
public:
	Ym2151_Emu();
	~Ym2151_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( double sample_rate, double clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 8 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write( int addr, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
