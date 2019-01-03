// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Kss_Scc_Apu.h"

/* Copyright (C) 2006-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

// Tones above this frequency are treated as disabled tone at half volume.
// Power of two is more efficient (avoids division).
int const inaudible_freq = 16384;

int const wave_size = 0x20;

void Scc_Apu::set_output( Blip_Buffer* buf )
{
	for ( int i = 0; i < osc_count; ++i )
		set_output( i, buf );
}

void Scc_Apu::volume( double v )
{
	synth.volume( 0.43 / osc_count / amp_range * v );
}

void Scc_Apu::reset()
{
	last_time = 0;

	for ( int i = osc_count; --i >= 0; )
		memset( &oscs [i], 0, offsetof (osc_t,output) );

	memset( regs, 0, sizeof regs );
}

Scc_Apu::Scc_Apu()
{
	set_output( NULL );
	volume( 1.0 );
	reset();
}

void Scc_Apu::run_until( blip_time_t end_time )
{
	for ( int index = 0; index < osc_count; index++ )
	{
		osc_t& osc = oscs [index];

		Blip_Buffer* const output = osc.output;
		if ( !output )
			continue;

		blip_time_t period = (regs [0xA0 + index * 2 + 1] & 0x0F) * 0x100 +
				regs [0xA0 + index * 2] + 1;
		int volume = 0;
		if ( regs [0xAF] & (1 << index) )
		{
			blip_time_t inaudible_period = (unsigned) (output->clock_rate() +
					inaudible_freq * 32) / (unsigned) (inaudible_freq * 16);
			if ( period > inaudible_period )
				volume = (regs [0xAA + index] & 0x0F) * (amp_range / 256 / 15);
		}

		BOOST::int8_t const* wave = (BOOST::int8_t*) regs + index * wave_size;
		/*if ( index == osc_count - 1 )
			wave -= wave_size; // last two oscs share same wave RAM*/

		{
			int delta = wave [osc.phase] * volume - osc.last_amp;
			if ( delta )
			{
				osc.last_amp += delta;
				output->set_modified();
				synth.offset( last_time, delta, output );
			}
		}

		blip_time_t time = last_time + osc.delay;
		if ( time < end_time )
		{
			int phase = osc.phase;
			if ( !volume )
			{
				// maintain phase
				int count = (end_time - time + period - 1) / period;
				phase += count; // will be masked below
				time  += count * period;
			}
			else
			{
				int last_wave = wave [phase];
				phase = (phase + 1) & (wave_size - 1); // pre-advance for optimal inner loop
				do
				{
					int delta = wave [phase] - last_wave;
					phase = (phase + 1) & (wave_size - 1);
					if ( delta )
					{
						last_wave += delta;
						synth.offset_inline( time, delta * volume, output );
					}
					time += period;
				}
				while ( time < end_time );

				osc.last_amp = last_wave * volume;
				output->set_modified();
				phase--; // undo pre-advance
			}
			osc.phase = phase & (wave_size - 1);
		}
		osc.delay = time - end_time;
	}
	last_time = end_time;
}
