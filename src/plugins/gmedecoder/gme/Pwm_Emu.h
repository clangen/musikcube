// PWM sound chip emulator interface

// Game_Music_Emu $vers
#ifndef PWM_EMU_H
#define PWM_EMU_H

class Pwm_Emu  {
	void* chip;
public:
	Pwm_Emu();
	~Pwm_Emu();
	
	// Sets output sample rate and chip clock rates, in Hz. Returns non-zero
	// if error.
	int set_rate( int clock );
	
	// Resets to power-up state
	void reset();
	
	// Mutes voice n if bit n (1 << n) of mask is set
	enum { channel_count = 24 };
	void mute_voices( int mask );
	
	// Writes data to channel
	void write( int channel, int data );
	
	// Runs and writes pair_count*2 samples to output
	typedef short sample_t;
	enum { out_chan_count = 2 }; // stereo
	void run( int pair_count, sample_t* out );
};

#endif
