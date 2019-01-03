// Konami SCC sound chip emulator

// $package
#ifndef KSS_SCC_APU_H
#define KSS_SCC_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Scc_Apu {
public:
// Basics

	// Sets buffer to generate sound into, or 0 to mute.
	void set_output( Blip_Buffer* );

	// Emulates to time t, then writes data to reg
	enum { reg_count = 0xB0 }; // 0 <= reg < reg_count
	void write( blip_time_t t, int reg, int data );

	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );

// More features

	// Resets sound chip
	void reset();

	// Same as set_output(), but for a particular channel
	enum { osc_count = 5 };
	void set_output( int chan, Blip_Buffer* );

	// Set overall volume, where 1.0 is normal
	void volume( double );

	// Set treble equalization
	void treble_eq( blip_eq_t const& eq )   { synth.treble_eq( eq ); }

private:
	// noncopyable
	Scc_Apu( const Scc_Apu& );
	Scc_Apu& operator = ( const Scc_Apu& );


// Implementation
public:
	Scc_Apu();
	BLARGG_DISABLE_NOTHROW

private:
	enum { amp_range = 0x8000 };
	struct osc_t
	{
		int delay;
		int phase;
		int last_amp;
		Blip_Buffer* output;
	};
	osc_t oscs [osc_count];
	blip_time_t last_time;
	unsigned char regs [reg_count];
	Blip_Synth_Fast synth;

	void run_until( blip_time_t );
};

inline void Scc_Apu::set_output( int index, Blip_Buffer* b )
{
	assert( (unsigned) index < osc_count );
	oscs [index].output = b;
}

inline void Scc_Apu::write( blip_time_t time, int addr, int data )
{
	//assert( (unsigned) addr < reg_count );
	assert( ( addr >= 0x9800 && addr <= 0x988F ) || ( addr >= 0xB800 && addr <= 0xB8AF ) );
	run_until( time );

	addr -= 0x9800;
	if ( ( unsigned ) addr < 0x90 )
	{
	    if ( ( unsigned ) addr < 0x60 )
            regs [addr] = data;
        else if ( ( unsigned ) addr < 0x80 )
        {
            regs [addr] = regs[addr + 0x20] = data;
        }
        else if ( ( unsigned ) addr < 0x90 )
        {
            regs [addr + 0x20] = data;
        }
	}
	else
	{
	    addr -= 0xB800 - 0x9800;
	    if ( ( unsigned ) addr < 0xB0 )
            regs [addr] = data;
	}
}

inline void Scc_Apu::end_frame( blip_time_t end_time )
{
	if ( end_time > last_time )
		run_until( end_time );

	last_time -= end_time;
	assert( last_time >= 0 );
}

#endif
