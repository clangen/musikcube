// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "K053260_Emu.h"
#include "k053260.h"

K053260_Emu::K053260_Emu() { chip = 0; }

K053260_Emu::~K053260_Emu()
{
	if ( chip ) device_stop_k053260( chip );
}

int K053260_Emu::set_rate( int clock_rate )
{
	if ( chip )
	{
		device_stop_k053260( chip );
		chip = 0;
	}
	
	chip = device_start_k053260( clock_rate );
	if ( !chip )
		return 1;
	
	reset();
	return 0;
}

void K053260_Emu::reset()
{
	device_reset_k053260( chip );
	k053260_set_mute_mask( chip, 0 );
}

void K053260_Emu::write( int addr, int data )
{
	k053260_w( chip, addr, data);
}

void K053260_Emu::write_rom( int size, int start, int length, void * data )
{
	k053260_write_rom( chip, size, start, length, (const UINT8 *) data );
}

void K053260_Emu::mute_voices( int mask )
{
	k053260_set_mute_mask( chip, mask );
}

void K053260_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		k053260_update( chip, buffers, todo );

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
