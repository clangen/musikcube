// Sunsoft FME-7 sound emulator

// $package
#ifndef NES_FME7_APU_H
#define NES_FME7_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

struct fme7_apu_state_t
{
	enum { reg_count = 14 };
	BOOST::uint8_t regs [reg_count];
	BOOST::uint8_t phases [3]; // 0 or 1
	BOOST::uint8_t latch;
	BOOST::uint16_t delays [3]; // a, b, c
};

class Nes_Fme7_Apu : private fme7_apu_state_t {
public:
	// See Nes_Apu.h for reference
	void reset();
	void volume( double );
	void treble_eq( blip_eq_t const& );
	void set_output( Blip_Buffer* );
	enum { osc_count = 3 };
	void set_output( int index, Blip_Buffer* );
	void end_frame( blip_time_t );
	void save_state( fme7_apu_state_t* ) const;
	void load_state( fme7_apu_state_t const& );
	
	// Mask and addresses of registers
	enum { addr_mask = 0xE000 };
	enum { data_addr = 0xE000 };
	enum { latch_addr = 0xC000 };
	
	// (addr & addr_mask) == latch_addr
	void write_latch( int );
	
	// (addr & addr_mask) == data_addr
	void write_data( blip_time_t, int data );
	
public:
	Nes_Fme7_Apu();
	BLARGG_DISABLE_NOTHROW
private:
	// noncopyable
	Nes_Fme7_Apu( const Nes_Fme7_Apu& );
	Nes_Fme7_Apu& operator = ( const Nes_Fme7_Apu& );
	
	static unsigned char const amp_table [16];
	
	struct {
		Blip_Buffer* output;
		int last_amp;
	} oscs [osc_count];
	blip_time_t last_time;
	
	enum { amp_range = 192 }; // can be any value; this gives best error/quality tradeoff
	Blip_Synth_Norm synth;
	
	void run_until( blip_time_t );
};

inline void Nes_Fme7_Apu::volume( double v )
{
	synth.volume( 0.38 / amp_range * v ); // to do: fine-tune
}

inline void Nes_Fme7_Apu::treble_eq( blip_eq_t const& eq )
{
	synth.treble_eq( eq );
}

inline void Nes_Fme7_Apu::set_output( int i, Blip_Buffer* buf )
{
	assert( (unsigned) i < osc_count );
	oscs [i].output = buf;
}

inline void Nes_Fme7_Apu::set_output( Blip_Buffer* buf )
{
	for ( int i = 0; i < osc_count; ++i )
		set_output( i, buf );
}

inline Nes_Fme7_Apu::Nes_Fme7_Apu()
{
	set_output( NULL );
	volume( 1.0 );
	reset();
}

inline void Nes_Fme7_Apu::write_latch( int data ) { latch = data; }

inline void Nes_Fme7_Apu::write_data( blip_time_t time, int data )
{
	if ( (unsigned) latch >= reg_count )
	{
		#ifdef dprintf
			dprintf( "FME7 write to %02X (past end of sound registers)\n", (int) latch );
		#endif
		return;
	}
	
	run_until( time );
	regs [latch] = data;
}

inline void Nes_Fme7_Apu::end_frame( blip_time_t time )
{
	if ( time > last_time )
		run_until( time );
	
	assert( last_time >= time );
	last_time -= time;
}

inline void Nes_Fme7_Apu::save_state( fme7_apu_state_t* out ) const
{
	*out = *this;
}

inline void Nes_Fme7_Apu::load_state( fme7_apu_state_t const& in )
{
	reset();
	fme7_apu_state_t* state = this;
	*state = in;
}

#endif
