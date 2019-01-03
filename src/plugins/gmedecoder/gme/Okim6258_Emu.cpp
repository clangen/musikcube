// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Okim6258_Emu.h"
#include "okim6258.h"

Okim6258_Emu::Okim6258_Emu() { chip = 0; }

Okim6258_Emu::~Okim6258_Emu()
{
	if ( chip ) device_stop_okim6258( chip );
}

int Okim6258_Emu::set_rate( int clock, int divider, int adpcm_type, int output_12bits )
{
	if ( chip )
	{
		device_stop_okim6258( chip );
		chip = 0;
	}
	
	chip = device_start_okim6258( clock, divider, adpcm_type, output_12bits );
	if ( !chip )
		return 0;
	
	reset();
	return okim6258_get_vclk( chip );
}

void Okim6258_Emu::reset()
{
	device_reset_okim6258( chip );
}

void Okim6258_Emu::write( int addr, int data )
{
	okim6258_write( chip, addr, data );
}

int Okim6258_Emu::get_clock()
{
    return okim6258_get_vclk( chip );
}

void Okim6258_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		okim6258_update( chip, buffers, todo );

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
