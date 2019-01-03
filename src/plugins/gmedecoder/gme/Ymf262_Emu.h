// YMF262 FM sound chip emulator interface

// Game_Music_Emu $vers
#ifndef YMF262_EMU_H
#define YMF262_EMU_H

namespace DBOPL {
	struct Chip;
}

class Ymf262_Emu  {
	DBOPL::Chip * opl;
	unsigned sample_rate;
	unsigned clock_rate;
public:
	Ymf262_Emu();
	~Ymf262_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int sample_rate, int clock_rate );
	
	// Resets to power-up state
	void reset();
	
	// Writes data to addr
	void write0( int addr, int data );
	void write1( int addr, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
