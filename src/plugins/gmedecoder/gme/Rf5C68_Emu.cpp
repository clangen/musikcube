// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Rf5C68_Emu.h"
#include "rf5c68.h"

Rf5C68_Emu::Rf5C68_Emu() { chip = 0; }

Rf5C68_Emu::~Rf5C68_Emu()
{
	if ( chip ) device_stop_rf5c68( chip );
}

int Rf5C68_Emu::set_rate()
{
	if ( chip )
	{
		device_stop_rf5c68( chip );
		chip = 0;
	}
	
	chip = device_start_rf5c68();
	if ( !chip )
		return 1;
	
	reset();
	return 0;
}

void Rf5C68_Emu::reset()
{
	device_reset_rf5c68( chip );
	rf5c68_set_mute_mask( chip, 0 );
}

void Rf5C68_Emu::write( int addr, int data )
{
	rf5c68_w( chip, addr, data );
}

void Rf5C68_Emu::write_mem( int addr, int data )
{
	rf5c68_mem_w( chip, addr, data );
}

void Rf5C68_Emu::write_ram( int start, int length, void * data )
{
	rf5c68_write_ram( chip, start, length, (const UINT8 *) data );
}

void Rf5C68_Emu::mute_voices( int mask )
{
	rf5c68_set_mute_mask( chip, mask );
}

void Rf5C68_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		rf5c68_update( chip, buffers, todo );

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
