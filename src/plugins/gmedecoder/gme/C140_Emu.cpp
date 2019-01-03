// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "C140_Emu.h"
#include "c140.h"

C140_Emu::C140_Emu() { chip = 0; }

C140_Emu::~C140_Emu()
{
	if ( chip ) device_stop_c140( chip );
}

int C140_Emu::set_rate( int type, double sample_rate, double clock_rate )
{
	if ( chip )
	{
		device_stop_c140( chip );
		chip = 0;
	}
	
	chip = device_start_c140( sample_rate, clock_rate, type );
	if ( !chip )
		return 1;
	
	reset();
	return 0;
}

void C140_Emu::reset()
{
	device_reset_c140( chip );
	c140_set_mute_mask( chip, 0 );
}

void C140_Emu::write( int addr, int data )
{
	c140_w( chip, addr, data );
}

void C140_Emu::write_rom( int size, int start, int length, void * data )
{
	c140_write_rom( chip, size, start, length, (const UINT8 *) data );
}

void C140_Emu::mute_voices( int mask )
{
	c140_set_mute_mask( chip, mask );
}

void C140_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		c140_update( chip, buffers, todo );

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
