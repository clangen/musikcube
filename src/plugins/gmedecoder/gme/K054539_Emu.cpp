// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "K054539_Emu.h"
#include "k054539.h"

K054539_Emu::K054539_Emu() { chip = 0; }

K054539_Emu::~K054539_Emu()
{
	if ( chip ) device_stop_k054539( chip );
}

int K054539_Emu::set_rate( int clock_rate, int flags )
{
	if ( chip )
	{
		device_stop_k054539( chip );
		chip = 0;
	}
	
	chip = device_start_k054539( clock_rate );
	if ( !chip )
		return 1;

	k054539_init_flags( chip, flags );
	
	reset();
	return 0;
}

void K054539_Emu::reset()
{
	device_reset_k054539( chip );
	k054539_set_mute_mask( chip, 0 );
}

void K054539_Emu::write( int addr, int data )
{
	k054539_w( chip, addr, data);
}

void K054539_Emu::write_rom( int size, int start, int length, void * data )
{
	k054539_write_rom( chip, size, start, length, (const UINT8 *) data );
}

void K054539_Emu::mute_voices( int mask )
{
	k054539_set_mute_mask( chip, mask );
}

void K054539_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		k054539_update( chip, buffers, todo );

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
