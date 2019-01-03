// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ymf262_Emu.h"

#include <math.h>
#include "dbopl.h"

Ymf262_Emu::Ymf262_Emu() { opl = 0; }

Ymf262_Emu::~Ymf262_Emu()
{
	delete opl;
}

int Ymf262_Emu::set_rate( int sample_rate, int clock_rate )
{
	delete opl;
	opl = 0;
	
	opl = new DBOPL::Chip;
	if ( !opl )
		return 1;

	this->sample_rate = sample_rate;
	this->clock_rate = clock_rate;

	reset();
	return 0;
}

void Ymf262_Emu::reset()
{
	opl->Setup( clock_rate, sample_rate );
}

void Ymf262_Emu::write0( int addr, int data )
{
	opl->WriteReg( addr, data );
}

void Ymf262_Emu::write1( int addr, int data )
{
	opl->WriteReg( 0x100 + addr, data );
}

void Ymf262_Emu::run( int pair_count, sample_t* out )
{
	Bit32s buf[ 2048 ];

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;

		if ( opl->opl3Active )
		{
			opl->GenerateBlock3( todo, buf );

			for (int i = 0; i < todo; i++)
			{
				int output_l, output_r;
				output_l = buf [i * 2];
				output_r = buf [i * 2 + 1];
				output_l += out [0];
				output_r += out [1];
				if ( (short)output_l != output_l ) output_l = 0x7FFF ^ ( output_l >> 31 );
				if ( (short)output_r != output_r ) output_r = 0x7FFF ^ ( output_r >> 31 );
				out [0] = output_l;
				out [1] = output_r;
				out += 2;
			}
		}
		else
		{
			opl->GenerateBlock2( todo, buf );

			for (int i = 0; i < todo; i++)
			{
				int output_l, output_r;
				int output = buf [i];
				output_l = output + out [0];
				output_r = output + out [1];
				if ( (short)output_l != output_l ) output_l = 0x7FFF ^ ( output_l >> 31 );
				if ( (short)output_r != output_r ) output_r = 0x7FFF ^ ( output_r >> 31 );
				out [0] = output_l;
				out [1] = output_r;
				out += 2;
			}
		}

		pair_count -= todo;
	}
}
