// $package. http://www.slack.net/~ant/

#include "Resampler.h"

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

Resampler::Resampler()
{
	write_pos = 0;
	rate_     = 0;
}

Resampler::~Resampler() { }

void Resampler::clear()
{
	write_pos = 0;
	clear_();
}

inline int Resampler::resample_wrapper( sample_t out [], int* out_size,
		sample_t const in [], int in_size )
{
	assert( rate() );
	
	sample_t* out_ = out;
	int result = resample_( &out_, out + *out_size, in, in_size ) - in;
	assert( out_ <= out + *out_size );
	assert( result <= in_size );
	
	*out_size = out_ - out;
	return result;
}

int Resampler::resample( sample_t out [], int out_size, sample_t const in [], int* in_size )
{
	*in_size = resample_wrapper( out, &out_size, in, *in_size );
	return out_size;
}


//// Buffering

blargg_err_t Resampler::resize_buffer( int new_size )
{
	RETURN_ERR( buf.resize( new_size ) );
	clear();
	return blargg_ok;
}

int Resampler::skip_input( int count )
{
	write_pos -= count;
	if ( write_pos < 0 ) // occurs when downsampling
	{
		count += write_pos;
		write_pos = 0;
	}
	memmove( buf.begin(), &buf [count], write_pos * sizeof buf [0] );
	return count;
}

int Resampler::read( sample_t out [], int out_size )
{
	if ( out_size )
		skip_input( resample_wrapper( out, &out_size, buf.begin(), write_pos ) );
	return out_size;
}
