// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ym2612_Emu.h"
#include "fm.h"

#include "blargg_errors.h"

// Ym2612_Emu

Ym2612_Emu::~Ym2612_Emu()
{
	if ( impl )
		ym2612_shutdown( impl );
}

const char* Ym2612_Emu::set_rate( double sample_rate, double clock_rate )
{
	if ( impl )
	{
		ym2612_shutdown( impl );
		impl = 0;
	}

	if ( !clock_rate )
		clock_rate = sample_rate * 144.;

	impl = ym2612_init( (long) (clock_rate + 0.5), (long) (sample_rate + 0.5) );
	if ( !impl )
		return blargg_err_memory;
	
	return 0;
}

void Ym2612_Emu::reset()
{
	ym2612_reset_chip( impl );
}

static stream_sample_t* DUMMYBUF[0x02] = {(stream_sample_t*)NULL, (stream_sample_t*)NULL};

void Ym2612_Emu::write0( int addr, int data )
{
	ym2612_update_one( impl, DUMMYBUF, 0 );
	ym2612_write( impl, 0, addr );
	ym2612_write( impl, 1, data );
}

void Ym2612_Emu::write1( int addr, int data )
{
	ym2612_update_one( impl, DUMMYBUF, 0 );
	ym2612_write( impl, 2, addr );
	ym2612_write( impl, 3, data );
}

void Ym2612_Emu::mute_voices( int mask )
{
	ym2612_set_mutemask( impl, mask );
}

void Ym2612_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		ym2612_update_one( impl, buffers, todo );

		for (int i = 0; i < todo; i++)
		{
			int output_l = bufL [i];
			int output_r = bufR [i];
			output_l += out [0];
			output_r += out [1];
			if ( (short)output_l != output_l ) output_l = 0x7FFF ^ ( output_l >> 31 );
			if ( (short)output_r != output_r ) output_r = 0x7FFF ^ ( output_r >> 31 );
			out [0] = output_l;
			out [1] = output_r;
			out += 2;
		}

		pair_count -= todo;
	}
}
