// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ymz280b_Emu.h"
#include "ymz280b.h"

Ymz280b_Emu::Ymz280b_Emu() { chip = 0; }

Ymz280b_Emu::~Ymz280b_Emu()
{
	if ( chip ) device_stop_ymz280b( chip );
}

int Ymz280b_Emu::set_rate( int clock_rate )
{
	if ( chip )
	{
		device_stop_ymz280b( chip );
		chip = 0;
	}
	
	chip = device_start_ymz280b( clock_rate );
	if ( !chip )
		return 0;
	
	reset();
	return clock_rate * 2 / 384;
}

void Ymz280b_Emu::reset()
{
	device_reset_ymz280b( chip );
	ymz280b_set_mute_mask( chip, 0 );
}

void Ymz280b_Emu::write( int addr, int data )
{
	ymz280b_w( chip, 0, addr );
	ymz280b_w( chip, 1, data );
}

void Ymz280b_Emu::write_rom( int size, int start, int length, void * data )
{
	ymz280b_write_rom( chip, size, start, length, (const UINT8 *) data );
}

void Ymz280b_Emu::mute_voices( int mask )
{
	ymz280b_set_mute_mask( chip, mask );
}

void Ymz280b_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		ymz280b_update( chip, buffers, todo );

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
