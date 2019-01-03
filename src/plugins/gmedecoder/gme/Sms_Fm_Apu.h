#ifndef SMS_FM_APU_H
#define SMS_FM_APU_H

#include "Blip_Buffer.h"
#include "Ym2413_Emu.h"

class Sms_Fm_Apu {
public:
	static bool supported()                     { return Ym2413_Emu::supported(); }
	blargg_err_t init( double clock_rate, double sample_rate );
	
	void set_output( Blip_Buffer* b, Blip_Buffer* = NULL, Blip_Buffer* = NULL ) { output_ = b; }
	void volume( double v )                     { synth.volume( 0.4 / 4096 * v ); }
	void treble_eq( blip_eq_t const& eq )       { synth.treble_eq( eq ); }
	
	void reset();
	
	void write_addr( int data )                 { addr = data; }
	void write_data( blip_time_t, int data );
	
	void end_frame( blip_time_t t );

// Implementation
public:
	Sms_Fm_Apu();
	~Sms_Fm_Apu();
	BLARGG_DISABLE_NOTHROW
	enum { osc_count = 1 };
	void set_output( int i, Blip_Buffer* b, Blip_Buffer* = NULL, Blip_Buffer* = NULL ) { output_ = b; }

private:
	Blip_Buffer* output_;
	blip_time_t next_time;
	int last_amp;
	int addr;
	
	int clock_;
	int rate_;
	blip_time_t period_;
	
	Blip_Synth_Norm synth;
	Ym2413_Emu apu;
	
	void run_until( blip_time_t );
};

#endif
