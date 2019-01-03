// NES FDS sound chip emulator

// $package
#ifndef NES_FDS_APU_H
#define NES_FDS_APU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"

class Nes_Fds_Apu {
public:
	// setup
	void set_tempo( double );
	enum { osc_count = 1 };
	void set_output( Blip_Buffer* buf );
	void volume( double );
	void treble_eq( blip_eq_t const& eq ) { synth.treble_eq( eq ); }
	
	// emulation
	void reset();
	enum { io_addr = 0x4040 };
	enum { io_size = 0x53 };
	void write( blip_time_t time, unsigned addr, int data );
	int read( blip_time_t time, unsigned addr );
	void end_frame( blip_time_t );
	
public:
	Nes_Fds_Apu();
	void write_( unsigned addr, int data );
	BLARGG_DISABLE_NOTHROW
	
	void set_output( int index, Blip_Buffer* center,
			Blip_Buffer* left_ignored = NULL, Blip_Buffer* right_ignored = NULL );
	BLARGG_DEPRECATED_TEXT( enum { start_addr = 0x4040 }; )
	BLARGG_DEPRECATED_TEXT( enum { end_addr = 0x4092 }; )
	BLARGG_DEPRECATED_TEXT( enum { reg_count = end_addr - start_addr + 1 }; )
	void osc_output( int, Blip_Buffer* );
private:
	enum { wave_size       = 0x40 };
	enum { master_vol_max  =   10 };
	enum { vol_max         = 0x20 };
	enum { wave_sample_max = 0x3F };
	
	unsigned char regs_ [io_size];// last written value to registers
	
	enum { lfo_base_tempo = 8 };
	int lfo_tempo; // normally 8; adjusted by set_tempo()   
	
	int env_delay;
	int env_speed;
	int env_gain;
	
	int sweep_delay;
	int sweep_speed;
	int sweep_gain;
	
	int wave_pos;
	int last_amp;
	blip_time_t wave_fract;
	
	int mod_fract;
	int mod_pos;
	int mod_write_pos;
	unsigned char mod_wave [wave_size];
	
	// synthesis
	blip_time_t last_time;
	Blip_Buffer* output_;
	Blip_Synth_Fast synth;
	
	// allow access to registers by absolute address (i.e. 0x4080)
	unsigned char& regs( unsigned addr ) { return regs_ [addr - io_addr]; }
	
	void run_until( blip_time_t );
};

inline void Nes_Fds_Apu::volume( double v )
{
	synth.volume( 0.14 / master_vol_max / vol_max / wave_sample_max * v );
}

inline void Nes_Fds_Apu::set_output( Blip_Buffer* b )
{
	output_ = b;
}

inline void Nes_Fds_Apu::set_output( int i, Blip_Buffer* buf, Blip_Buffer*, Blip_Buffer* )
{
	assert( (unsigned) i < osc_count );
	output_ = buf;
}

inline void Nes_Fds_Apu::end_frame( blip_time_t end_time )
{
	if ( end_time > last_time )
		run_until( end_time );
	last_time -= end_time;
	assert( last_time >= 0 );
}

inline void Nes_Fds_Apu::write( blip_time_t time, unsigned addr, int data )
{
	run_until( time );
	write_( addr, data );
}

inline int Nes_Fds_Apu::read( blip_time_t time, unsigned addr )
{
	run_until( time );
	
	int result = 0xFF;
	switch ( addr )
	{
	case 0x4090:
		result = env_gain;
		break;
	
	case 0x4092:
		result = sweep_gain;
		break;
	
	default:
		unsigned i = addr - io_addr;
		if ( i < wave_size )
			result = regs_ [i];
	}
	
	return result | 0x40;
}

inline Nes_Fds_Apu::Nes_Fds_Apu()
{
	lfo_tempo = lfo_base_tempo;
	set_output( NULL );
	volume( 1.0 );
	reset();
}

#endif
