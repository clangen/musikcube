// $package. http://www.slack.net/~ant/

#include "Upsampler.h"

/* Copyright (C) 2004-2008 Shay Green. This module is free software; you
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

int const shift = 15;
int const unit = 1 << shift;

void Upsampler::clear_()
{
	pos = 0;
	Resampler::clear_();
}

Upsampler::Upsampler()
{
	clear();
}

blargg_err_t Upsampler::set_rate_( double new_factor )
{
	step = (int) (new_factor * unit + 0.5);
	return Resampler::set_rate_( 1.0 / unit * step );
}

Resampler::sample_t const* Upsampler::resample_( sample_t** out_, sample_t const* out_end,
		sample_t const in [], int in_size )
{
	in_size -= write_offset;
	if ( in_size > 0 )
	{
		sample_t* BLARGG_RESTRICT out = *out_;
		sample_t const* const in_end = in + in_size;
		
		int const step = this->step;
		int       pos  = this->pos;
		
		do
		{
			#define INTERP( i, out )\
			{\
				int t = in [0 + i] * (unit - pos) + in [stereo + i] * pos;\
				out = t >> shift;\
			}
			
			int out_0;
			INTERP( 0,                  out_0 )
			INTERP( 1, out [0] = out_0; out [1] )
			out += stereo;
			
			pos += step;
			in  += ((unsigned) pos >> shift) * stereo;
			pos &= unit - 1;
		}
		while ( in < in_end && out < out_end );
		
		this->pos = pos;
		*out_ = out;
	}
	return in;
}
