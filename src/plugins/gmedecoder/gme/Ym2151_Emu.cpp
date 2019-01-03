// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ym2151_Emu.h"
#include "ym2151.h"

Ym2151_Emu::Ym2151_Emu() { PSG = 0; }

Ym2151_Emu::~Ym2151_Emu()
{
	if ( PSG ) ym2151_shutdown( PSG );
}

int Ym2151_Emu::set_rate( double sample_rate, double clock_rate )
{
	if ( PSG )
	{
		ym2151_shutdown( PSG );
		PSG = 0;
	}
	
	PSG = ym2151_init( clock_rate, sample_rate );
	if ( !PSG )
		return 1;
	
	reset();
	return 0;
}

void Ym2151_Emu::reset()
{
	ym2151_reset_chip( PSG );
	ym2151_set_mask( PSG, 0 );
}

static stream_sample_t* DUMMYBUF[0x02] = {(stream_sample_t*)NULL, (stream_sample_t*)NULL};

void Ym2151_Emu::write( int addr, int data )
{
	ym2151_update_one( PSG, DUMMYBUF, 0 );
	ym2151_write_reg( PSG, addr, data );
}

void Ym2151_Emu::mute_voices( int mask )
{
	ym2151_set_mask( PSG, mask );
}

void Ym2151_Emu::run( int pair_count, sample_t* out )
{
	SAMP bufL[ 1024 ];
	SAMP bufR[ 1024 ];
	SAMP * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		ym2151_update_one( PSG, buffers, todo );

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
