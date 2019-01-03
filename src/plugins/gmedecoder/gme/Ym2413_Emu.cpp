// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ym2413_Emu.h"
#include "ym2413.h"

Ym2413_Emu::Ym2413_Emu() { opll = 0; }

Ym2413_Emu::~Ym2413_Emu()
{
	if ( opll ) ym2413_shutdown( opll );
}

int Ym2413_Emu::set_rate( int sample_rate, int clock_rate )
{
	if ( opll )
	{
		ym2413_shutdown( opll );
		opll = 0;
	}
	
	opll = ym2413_init( clock_rate, sample_rate, 0 );
	if ( !opll )
		return 1;
	
	reset();
	return 0;
}

void Ym2413_Emu::reset()
{
	ym2413_reset_chip( opll );
	ym2413_set_mask( opll, 0 );
}

static stream_sample_t* DUMMYBUF[0x02] = {(stream_sample_t*)NULL, (stream_sample_t*)NULL};

void Ym2413_Emu::write( int addr, int data )
{
	ym2413_update_one( opll, DUMMYBUF, 0 );
	ym2413_write( opll, 0, addr );
	ym2413_write( opll, 1, data );
}

void Ym2413_Emu::mute_voices( int mask )
{
	ym2413_set_mask( opll, mask );
}

void Ym2413_Emu::run( int pair_count, sample_t* out )
{
	SAMP bufMO[ 1024 ];
	SAMP bufRO[ 1024 ];
	SAMP * buffers[2] = { bufMO, bufRO };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		ym2413_update_one( opll, buffers, todo );

		for (int i = 0; i < todo; i++)
		{
			int output_l, output_r;
			int output = bufMO [i];
			output += bufRO [i];
			output *= 3;
			output_l = output + out [0];
			output_r = output + out [1];
			if ( (short)output_l != output_l ) output_l = 0x7FFF ^ ( output_l >> 31 );
			if ( (short)output_r != output_r ) output_r = 0x7FFF ^ ( output_r >> 31 );
			out [0] = output_l;
			out [1] = output_r;
			out += 2;
		}

		pair_count -= todo;
	}
}
