// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Okim6295_Emu.h"
#include "okim6295.h"

Okim6295_Emu::Okim6295_Emu() { chip = 0; }

Okim6295_Emu::~Okim6295_Emu()
{
	if ( chip ) device_stop_okim6295( chip );
}

int Okim6295_Emu::set_rate( int clock_rate )
{
	if ( chip )
	{
		device_stop_okim6295( chip );
		chip = 0;
	}
	
	chip = device_start_okim6295( clock_rate );
	if ( !chip )
		return 0;
	
	reset();
	return (clock_rate & 0x7FFFFFFF) / ((clock_rate & 0x80000000) ? 132 : 165);
}

void Okim6295_Emu::reset()
{
	device_reset_okim6295( chip );
	okim6295_set_mute_mask( chip, 0 );
}

void Okim6295_Emu::write( int addr, int data )
{
	okim6295_w( chip, addr, data );
}

void Okim6295_Emu::write_rom( int size, int start, int length, void * data )
{
	okim6295_write_rom( chip, size, start, length, (const UINT8 *) data );
}

void Okim6295_Emu::mute_voices( int mask )
{
	okim6295_set_mute_mask( chip, mask );
}

void Okim6295_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		okim6295_update( chip, buffers, todo );

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
