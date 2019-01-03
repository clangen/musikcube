// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Pwm_Emu.h"
#include "pwm.h"

Pwm_Emu::Pwm_Emu() { chip = 0; }

Pwm_Emu::~Pwm_Emu()
{
	if ( chip ) device_stop_pwm( chip );
}

int Pwm_Emu::set_rate( int clock )
{
	if ( chip )
	{
		device_stop_pwm( chip );
		chip = 0;
	}
	
	chip = device_start_pwm( clock );
	if ( !chip )
		return 1;
	
	reset();
	return 0;
}

void Pwm_Emu::reset()
{
	device_reset_pwm( chip );
}

void Pwm_Emu::write( int channel, int data )
{
	pwm_chn_w( chip, channel, data );
}

void Pwm_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		pwm_update( chip, buffers, todo );

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
