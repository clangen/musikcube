// Turbo Grafx 16 (PC Engine) ADPCM sound chip emulator

// Game_Music_Emu $vers
#ifndef HES_APU_ADPCM_H
#define HES_APU_ADPCM_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Hes_Apu_Adpcm {
public:
// Basics

	// Sets buffer(s) to generate sound into, or 0 to mute. If only center is not 0,
	// output is mono.
	void set_output( Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );

	// Emulates to time t, then writes data to addr
	void write_data( blip_time_t t, int addr, int data );

	// Emulates to time t, then reads from addr
	int read_data( blip_time_t t, int addr );
	
	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );
	
// More features
	
	// Resets sound chip
	void reset();
	
	// Same as set_output(), but for a particular channel
	enum { osc_count = 1 }; // 0 <= chan < osc_count
	void set_output( int chan, Blip_Buffer* center, Blip_Buffer* left = NULL, Blip_Buffer* right = NULL );
	
	// Sets treble equalization
	void treble_eq( blip_eq_t const& eq )   { synth.treble_eq( eq ); }
	
	// Sets overall volume, where 1.0 is normal
	void volume( double v )                 { synth.volume( 0.6 / osc_count / amp_range * v ); }
	
	// Registers are at io_addr to io_addr+io_size-1
	enum { io_addr = 0x1800 };
	enum { io_size = 0x400 };
	
// Implementation
public:
	Hes_Apu_Adpcm();
	typedef BOOST::uint8_t byte;

private:
	enum { amp_range = 2048 };

	struct State
	{
		byte           pcmbuf [0x10000];
		byte           port [0x10];
		int            ad_sample;
		int            ad_ref_index;
		bool           ad_low_nibble;
		int            freq;
		unsigned short addr;
		unsigned short writeptr;
		unsigned short readptr;
		unsigned short playptr;
		byte           playflag;
		byte           repeatflag;
		int            length;
		int            playlength;
		int            playedsamplecount;
		int            volume;
		int            fadetimer;
		int            fadecount;
	};
	State state;
	Blip_Synth_Fast synth;

	Blip_Buffer* output;
	blip_time_t  last_time;
	double       next_timer;
	int          last_amp;

	void run_until( blip_time_t );

	int adpcm_decode( int );
};

inline void Hes_Apu_Adpcm::set_output( Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	set_output( 0, c, l, r );
}

#endif
