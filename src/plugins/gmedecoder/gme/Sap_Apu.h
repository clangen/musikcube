// Atari POKEY sound chip emulator

// Game_Music_Emu $vers
#ifndef SAP_APU_H
#define SAP_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Sap_Apu_Impl;

class Sap_Apu {
public:
// Basics

	// Sets buffer to generate sound into, or 0 to mute
	void set_output( Blip_Buffer* );
	
	// Emulates to time t, then writes data to addr
	void write_data( blip_time_t t, int addr, int data );

	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );
	
// More features

	// Same as set_output(), but for a particular channel
	enum { osc_count = 4 };
	void set_output( int index, Blip_Buffer* );
	
	// Resets sound chip and sets Sap_Apu_Impl
	void reset( Sap_Apu_Impl* impl );
	
	// Registers are at io_addr to io_addr+io_size-1
	enum { io_addr = 0xD200 };
	enum { io_size = 0x0A };
	
private:
	// noncopyable
	Sap_Apu( const Sap_Apu& );
	Sap_Apu& operator = ( const Sap_Apu& );
	
// Implementation
public:
	Sap_Apu();
	
private:
	struct osc_t
	{
		unsigned char regs [2];
		unsigned char phase;
		unsigned char invert;
		int last_amp;
		blip_time_t delay;
		blip_time_t period; // always recalculated before use; here for convenience
		Blip_Buffer* output;
	};
	osc_t oscs [osc_count];
	Sap_Apu_Impl* impl;
	blip_time_t last_time;
	int poly5_pos;
	int poly4_pos;
	int polym_pos;
	int control;
	
	void calc_periods();
	void run_until( blip_time_t );
	
	enum { poly4_len  = (1 <<  4) - 1 };
	enum { poly9_len  = (1 <<  9) - 1 };
	enum { poly17_len = (1 << 17) - 1 };
	friend class Sap_Apu_Impl;
};

// Common tables and Blip_Synth that can be shared among multiple Sap_Apu objects
class Sap_Apu_Impl {
public:
	// Set treble with synth.treble_eq()
	Blip_Synth_Norm synth;
	
	// Sets overall volume, where 1.0is normal
	void volume( double d ) { synth.volume( 1.0 / Sap_Apu::osc_count / 30 * d ); }
	
	
// Implementation
public:
	Sap_Apu_Impl();

private:
	BOOST::uint8_t poly4  [Sap_Apu::poly4_len /8 + 1];
	BOOST::uint8_t poly9  [Sap_Apu::poly9_len /8 + 1];
	BOOST::uint8_t poly17 [Sap_Apu::poly17_len/8 + 1];
	friend class Sap_Apu;
};

inline void Sap_Apu::set_output( int i, Blip_Buffer* b )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = b;
}

#endif
