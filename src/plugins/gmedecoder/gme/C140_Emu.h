// C140 sound chip emulator interface

// Game_Music_Emu $vers
#ifndef C140_EMU_H
#define C140_EMU_H

class C140_Emu  {
	void* chip;
public:
	C140_Emu();
	~C140_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int type, double sample_rate, double clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 24 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write( int addr, int data );

	// Scales ROM size, then writes length bytes from data at start offset
	void write_rom( int size, int start, int length, void * data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
