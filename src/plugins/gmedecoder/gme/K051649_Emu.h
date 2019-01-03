// K051649 sound chip emulator interface

// Game_Music_Emu $vers
#ifndef K051649_EMU_H
#define K051649_EMU_H

class K051649_Emu  {
	void* SCC;
public:
	K051649_Emu();
	~K051649_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 5 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write( int port, int offset, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
