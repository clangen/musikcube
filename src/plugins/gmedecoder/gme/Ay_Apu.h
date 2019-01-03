// AY-3-8910 sound chip emulator

// $package
#ifndef AY_APU_H
#define AY_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Ay_Apu {
public:
// Basics
	enum Ay_Apu_Type
	{
		Ay8910 = 0,
		Ay8912,
		Ay8913,
		Ay8914,
		Ym2149 = 0x10,
		Ym3439,
		Ymz284,
		Ymz294,
		Ym2203 = 0x20,
		Ym2608,
		Ym2610,
		Ym2610b
	};

	void set_type( Ay_Apu_Type type ) { type_ = type; }

	// Sets buffer to generate sound into, or 0 to mute.
	void set_output( Blip_Buffer* );
	
	// Writes to address register
	void write_addr( int data )                 { addr_ = data & 0x0F; }
	
	// Emulates to time t, then writes to current data register
	void write_data( blip_time_t t, int data )  { run_until( t ); write_data_( addr_, data ); }
	
	// Emulates to time t, then subtracts t from the current time.
	// OK if previous write call had time slightly after t.
	void end_frame( blip_time_t t );
	
// More features
	
	// Reads from current data register
	int read();
	
	// Resets sound chip
	void reset();
	
	// Number of registers
	enum { reg_count = 16 };
	
	// Same as set_output(), but for a particular channel
	enum { osc_count = 3 };
	void set_output( int chan, Blip_Buffer* );
	
	// Sets overall volume, where 1.0 is normal
	void volume( double v )                     { synth_.volume( 0.7/osc_count/amp_range * v ); }
	
	// Sets treble equalization
	void treble_eq( blip_eq_t const& eq )       { synth_.treble_eq( eq ); }
	
private:
	// noncopyable
	Ay_Apu( const Ay_Apu& );
	Ay_Apu& operator = ( const Ay_Apu& );

// Implementation
public:
	Ay_Apu();
	BLARGG_DISABLE_NOTHROW
	typedef BOOST::uint8_t byte;

private:
	struct osc_t
	{
		blip_time_t  period;
		blip_time_t  delay;
		short        last_amp;
		short        phase;
		Blip_Buffer* output;
	} oscs [osc_count];

	Ay_Apu_Type type_;

	blip_time_t last_time;
	byte        addr_;
	byte        regs [reg_count];
	
	blip_time_t noise_delay;
	unsigned    noise_lfsr;
	
	blip_time_t env_delay;
	byte const* env_wave;
	int         env_pos;
	byte        env_modes [8] [48]; // values already passed through volume table
	
	void write_data_( int addr, int data );
	void run_until( blip_time_t );
	
public:
	enum { amp_range = 255 };
	Blip_Synth_Norm synth_; // used by Ay_Core for beeper sound
};

inline void Ay_Apu::set_output( int i, Blip_Buffer* out )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = out;
}

inline void Ay_Apu::end_frame( blip_time_t time )
{
	if ( time > last_time )
		run_until( time );
	
	last_time -= time;
	assert( last_time >= 0 );
}

#endif
