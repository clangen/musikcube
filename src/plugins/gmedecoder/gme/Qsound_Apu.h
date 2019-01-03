// Capcom QSound sound chip emulator interface

// Game_Music_Emu $vers
#ifndef QSOUND_APU_H
#define QSOUND_APU_H

class Qsound_Apu  {
	void* chip;
    void* rom;
    int rom_size;
	int sample_rate;
public:
    Qsound_Apu();
    ~Qsound_Apu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock_rate );
	void set_sample_rate( int sample_rate );
	
	// Resets to power-up state
	void reset();
	
	// Writes data to addr
	void write( int addr, int data );

	// Scales ROM size, then writes length bytes from data at start offset
    void write_rom( int size, int start, int length, void const* data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
