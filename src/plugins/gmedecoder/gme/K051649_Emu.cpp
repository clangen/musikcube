// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "K051649_Emu.h"
#include "k051649.h"

K051649_Emu::K051649_Emu() { SCC = 0; }

K051649_Emu::~K051649_Emu()
{
	if ( SCC ) device_stop_k051649( SCC );
}

int K051649_Emu::set_rate( int clock_rate )
{
	if ( SCC )
	{
		device_stop_k051649( SCC );
		SCC = 0;
	}
	
	SCC = device_start_k051649( clock_rate );
	if ( !SCC )
		return 1;
	
	reset();
	return 0;
}

void K051649_Emu::reset()
{
	device_reset_k051649( SCC );
	k051649_set_mute_mask( SCC, 0 );
}

void K051649_Emu::write( int port, int offset, int data )
{
	k051649_w( SCC, (port << 1) | 0x00, offset);
	k051649_w( SCC, (port << 1) | 0x01, data);
}

void K051649_Emu::mute_voices( int mask )
{
	k051649_set_mute_mask( SCC, mask );
}

void K051649_Emu::run( int pair_count, sample_t* out )
{
	stream_sample_t bufL[ 1024 ];
	stream_sample_t bufR[ 1024 ];
	stream_sample_t * buffers[2] = { bufL, bufR };

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
		k051649_update( SCC, buffers, todo );

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
