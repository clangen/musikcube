// $package. http://www.slack.net/~ant/

#include "Fir_Resampler.h"

#include <math.h>

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

#undef PI
#define PI 3.1415926535897932384626433832795029

static void gen_sinc( double rolloff, int width, double offset, double spacing, double scale,
		int count, short* out )
{
	double const maxh = 256;
	double const step = PI / maxh * spacing;
	double const to_w = maxh * 2 / width;
	double const pow_a_n = pow( rolloff, maxh );
	scale /= maxh * 2;
	
	double angle = (count / 2 - 1 + offset) * -step;
	while ( count-- )
	{
		*out++ = 0;
		double w = angle * to_w;
		if ( fabs( w ) < PI )
		{
			double rolloff_cos_a = rolloff * cos( angle );
			double num = 1 - rolloff_cos_a -
					pow_a_n * cos( maxh * angle ) +
					pow_a_n * rolloff * cos( (maxh - 1) * angle );
			double den = 1 - rolloff_cos_a - rolloff_cos_a + rolloff * rolloff;
			double sinc = scale * num / den - scale;
			
			out [-1] = (short) (cos( w ) * sinc + sinc);
		}
		angle += step;
	}
}

Fir_Resampler_::Fir_Resampler_( int width, sample_t impulses_ [] ) :
	width_( width ),
	impulses( impulses_ )
{
	imp = NULL;
}

void Fir_Resampler_::clear_()
{
	imp = impulses;
	Resampler::clear_();
}

blargg_err_t Fir_Resampler_::set_rate_( double new_factor )
{
	double const rolloff = 0.999;
	double const gain = 1.0;
	
	// determine number of sub-phases that yield lowest error
	double ratio_ = 0.0;
	int res = -1;
	{
		double least_error = 2;
		double pos = 0;
		for ( int r = 1; r <= max_res; r++ )
		{
			pos += new_factor;
			double nearest = floor( pos + 0.5 );
			double error = fabs( pos - nearest );
			if ( error < least_error )
			{
				res = r;
				ratio_ = nearest / res;
				least_error = error;
			}
		}
	}
	RETURN_ERR( Resampler::set_rate_( ratio_ ) );
	
	// how much of input is used for each output sample
	int const step = stereo * (int) floor( ratio_ );
	double fraction = fmod( ratio_, 1.0 );
	
	double const filter = (ratio_ < 1.0) ? 1.0 : 1.0 / ratio_;
	double pos = 0.0;
	//int input_per_cycle = 0;
	sample_t* out = impulses;
	for ( int n = res; --n >= 0; )
	{
		gen_sinc( rolloff, int (width_ * filter + 1) & ~1, pos, filter,
				double (0x7FFF * gain * filter), (int) width_, out );
		out += width_;
		
		int cur_step = step;
		pos += fraction;
		if ( pos >= 0.9999999 )
		{
			pos -= 1.0;
			cur_step += stereo;
		}
		
		*out++ = (cur_step - width_ * 2 + 4) * sizeof (sample_t);
		*out++ = 4 * sizeof (sample_t);
		//input_per_cycle += cur_step;
	}
	// last offset moves back to beginning of impulses
	out [-1] -= (char*) out - (char*) impulses;
	
	imp = impulses;
	
	return blargg_ok;
}
