#include "Sms_Fm_Apu.h"

#include "blargg_source.h"

Sms_Fm_Apu::Sms_Fm_Apu()
{ }

Sms_Fm_Apu::~Sms_Fm_Apu()
{ }

blargg_err_t Sms_Fm_Apu::init( double clock_rate, double sample_rate )
{
	period_ = clock_rate / sample_rate + 0.5;
	CHECK_ALLOC( !apu.set_rate( sample_rate, clock_rate ) );
	
	set_output( 0 );
	volume( 1.0 );
	reset();
	return blargg_ok;
}

void Sms_Fm_Apu::reset()
{
	addr      = 0;
	next_time = 0;
	last_amp  = 0;
	
	apu.reset();
}

void Sms_Fm_Apu::write_data( blip_time_t time, int data )
{
	if ( time > next_time )
		run_until( time );
	
	apu.write( addr, data );
}

void Sms_Fm_Apu::run_until( blip_time_t end_time )
{
	assert( end_time > next_time );
	
	Blip_Buffer* const output = this->output_;
	if ( !output )
	{
		next_time = end_time;
		return;
	}
	
	blip_time_t time = next_time;
	do
	{
		Ym2413_Emu::sample_t samples [2] = {0};
		apu.run( 1, samples );
		int amp = (samples [0] + samples [1]) >> 1;
		
		int delta = amp - last_amp;
		if ( delta )
		{
			last_amp = amp;
			synth.offset_inline( time, delta, output );
		}
		time += period_;
	}
	while ( time < end_time );
	
	next_time = time;
}

void Sms_Fm_Apu::end_frame( blip_time_t time )
{
	if ( time > next_time )
		run_until( time );
	
	next_time -= time;
	assert( next_time >= 0 );
	
	if ( output_ )
		output_->set_modified();
}
