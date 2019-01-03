// RF5C164 sound chip emulator interface

// Game_Music_Emu $vers
#ifndef RF5C164_EMU_H
#define RF5C164_EMU_H

class Rf5C164_Emu  {
	void* chip;
public:
	Rf5C164_Emu();
	~Rf5C164_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 8 };
	void mute_voices( int mask );
	
	// Writes data to addr
	void write( int addr, int data );

	// Writes to memory
	void write_mem( int addr, int data );

	// Writes length bytes from data at start offset in RAM
	void write_ram( int start, int length, void * data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
