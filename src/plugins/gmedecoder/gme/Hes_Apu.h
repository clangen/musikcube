// Turbo Grafx 16 (PC Engine) PSG sound chip emulator

// Game_Music_Emu $vers
#ifndef HES_APU_H
#define HES_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Hes_Apu {
public:
// Basics

	// Sets buffer(s) to generate sound into, or 0 to mute. If only center is not 0,
	// output is mono.
	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );

	// Emulates to time t, then writes data to addr
	void write_data( blip_time_t t, int addr, int data );
	
	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );
	
// More features
	
	// Resets sound chip
	void reset();
	
	// Same as set_output(), but for a particular channel
	enum { osc_count = 6 }; // 0 <= chan < osc_count
	void set_output( int chan, Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );
	
	// Sets treble equalization
	void treble_eq( blip_eq_t const& eq )   { synth.treble_eq( eq ); }
	
	// Sets overall volume, where 1.0 is normal
	void volume( double v )                 { synth.volume( 1.8 / osc_count / amp_range * v ); }
	
	// Registers are at io_addr to io_addr+io_size-1
	enum { io_addr = 0x0800 };
	enum { io_size = 10 };
	
// Implementation
public:
	Hes_Apu();
	typedef BOOST::uint8_t byte;

private:
	enum { amp_range = 0x8000 };
	struct Osc
	{
		byte wave [32];
		int  delay;
		int  period;
		int  phase;
		
		int  noise_delay;
		byte noise;
		unsigned lfsr;
		
		byte control;
		byte balance;
		byte dac;
		short volume [2];
		int last_amp [2];
		
		blip_time_t last_time;
		Blip_Buffer* output [2];
		Blip_Buffer* outputs [3];
	};
	Osc oscs [osc_count];
	int latch;
	int balance;
	Blip_Synth_Fast synth;
	
	void balance_changed( Osc& );
	static void run_osc( Blip_Synth_Fast&, Osc&, blip_time_t );
};

inline void Hes_Apu::set_output( Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	for ( int i = osc_count; --i >= 0; )
		set_output( i, c, l, r );
}

#endif
