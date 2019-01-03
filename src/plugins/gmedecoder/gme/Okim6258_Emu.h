// OKIM6258 sound chip emulator interface

// Game_Music_Emu $vers
#ifndef OKIM6258_EMU_H
#define OKIM6258_EMU_H

class Okim6258_Emu  {
	void* chip;
public:
	Okim6258_Emu();
	~Okim6258_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock, int divider, int adpcm_type, int output_12bits );
	
	// Resets to power-up state
	void reset();
    
    // Returns current sample rate of the chip
    int get_clock();
	
	// Writes data to addr
	void write( int addr, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
