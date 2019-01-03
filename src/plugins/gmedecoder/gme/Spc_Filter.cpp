// snes_spc $vers. http://www.slack.net/~ant/

#include "Spc_Filter.h"

/* Copyright (C) 2007 Shay Green. This module is free software; you
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

static const float limiter_max = (float)0.9999;
static const float limiter_max_05 = (float)(limiter_max - 0.5);

#include <math.h>

static short hard_limit_sample( int sample )
{
    double val = (double)sample * (1.0 / 32768.0);
    if (val < -0.5)
        val = (tanh((val + 0.5) / limiter_max_05) * limiter_max_05 - 0.5);
    else if (val > 0.5)
        val = (tanh((val - 0.5) / limiter_max_05) * limiter_max_05 + 0.5);
    return val * 32768.0;
}

void Spc_Filter::build_limit_table()
{
    for (int i = -65536; i < 65536; ++i)
    {
        hard_limit_table[ i + 65536 ] = hard_limit_sample( i );
    }
}

inline short Spc_Filter::limit_sample(int sample)
{
    if (!limiting && ((unsigned)sample + 32768) < 65536) return sample;
    limiting = true;
    if (((unsigned)sample + 65536) < 131072) return hard_limit_table[ sample + 65536 ];
    return hard_limit_sample( sample );
}

void Spc_Filter::clear() { limiting = false; memset( ch, 0, sizeof ch ); }

Spc_Filter::Spc_Filter()
{
	enabled = true;
	gain    = gain_unit;
	bass    = bass_norm;
	clear();
    build_limit_table();
}

void Spc_Filter::run( short io [], int count )
{
	require( (count & 1) == 0 ); // must be even
	
	int const gain = this->gain;
	if ( enabled )
	{
		int const bass = this->bass;
		chan_t* c = &ch [2];
		do
		{
			// cache in registers
			int sum = (--c)->sum;
			int pp1 = c->pp1;
			int p1  = c->p1;
			
			for ( int i = 0; i < count; i += 2 )
			{
				// Low-pass filter (two point FIR with coeffs 0.25, 0.75)
				int f = io [i] + p1;
				p1 = io [i] * 3;
				
				// High-pass filter ("leaky integrator")
				int delta = f - pp1;
				pp1 = f;
				int s = sum >> (gain_bits + 2);
				sum += (delta * gain) - (sum >> bass);

				io [i] = limit_sample( s );
			}
			
			c->p1  = p1;
			c->pp1 = pp1;
			c->sum = sum;
			++io;
		}
		while ( c != ch );
	}
	else if ( gain != gain_unit )
	{
		short* const end = io + count;
		while ( io < end )
		{
			int s = (*io * gain) >> gain_bits;
			*io++ = limit_sample( s );
		}
	}
}
