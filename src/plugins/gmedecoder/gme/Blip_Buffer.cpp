// Blip_Buffer $vers. http://www.slack.net/~ant/

#include "Blip_Buffer.h"

#include <math.h>

/* Copyright (C) 2003-2008 Shay Green. This module is free software; you
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

//// Blip_Buffer

Blip_Buffer::Blip_Buffer()
{
	factor_      = UINT_MAX/2 + 1;
	buffer_      = NULL;
	buffer_center_ = NULL;
	buffer_size_ = 0;
	sample_rate_ = 0;
	bass_shift_  = 0;
	clock_rate_  = 0;
	bass_freq_   = 16;
	length_      = 0;
	
	// assumptions code makes about implementation-defined features
	#ifndef NDEBUG
		// right shift of negative value preserves sign
		int i = -0x7FFFFFFE;
		assert( (i >> 1) == -0x3FFFFFFF );
		
		// casting truncates and sign-extends
		i = 0x18000;
		assert( (BOOST::int16_t) i == -0x8000 );
	#endif
	
	clear();
}

Blip_Buffer::~Blip_Buffer()
{
	free( buffer_ );
}

void Blip_Buffer::clear()
{
	bool const entire_buffer = true;
	
	offset_       = 0;
	reader_accum_ = 0;
	modified_     = false;
	
	if ( buffer_ )
	{
		int count = (entire_buffer ? buffer_size_ : samples_avail());
		memset( buffer_, 0, (count + blip_buffer_extra_) * sizeof (delta_t) );
	}
}

blargg_err_t Blip_Buffer::set_sample_rate( int new_rate, int msec )
{
	// Limit to maximum size that resampled time can represent
	int max_size = (((blip_resampled_time_t) -1) >> BLIP_BUFFER_ACCURACY) -
			blip_buffer_extra_ - 64; // TODO: -64 isn't needed
	int new_size = (new_rate * (msec + 1) + 999) / 1000;
	if ( new_size > max_size )
		new_size = max_size;
	
	// Resize buffer
	if ( buffer_size_ != new_size )
	{
		//dprintf( "%d \n", (new_size + blip_buffer_extra_) * sizeof *buffer_  );
		void* p = realloc( buffer_, (new_size + blip_buffer_extra_) * sizeof *buffer_ );
		CHECK_ALLOC( p );
		buffer_      = (delta_t*) p;
		buffer_center_ = buffer_ + BLIP_MAX_QUALITY/2;
		buffer_size_ = new_size;
	}
	
	// Update sample_rate and things that depend on it
	sample_rate_ = new_rate;
	length_      = new_size * 1000 / new_rate - 1;
	if ( clock_rate_ )
		clock_rate( clock_rate_ );
	bass_freq( bass_freq_ );
	
	clear();
	
	return blargg_ok;
}

blip_resampled_time_t Blip_Buffer::clock_rate_factor( int rate ) const
{
	double ratio = (double) sample_rate_ / rate;
	int factor = (int) floor( ratio * (1 << BLIP_BUFFER_ACCURACY) + 0.5 );
	assert( factor > 0 || !sample_rate_ ); // fails if clock/output ratio is too large
	return (blip_resampled_time_t) factor;
}

void Blip_Buffer::bass_freq( int freq )
{
	bass_freq_ = freq;
	int shift = 31;
	if ( freq > 0 && sample_rate_ )
	{
		shift = 13;
		int f = (freq << 16) / sample_rate_;
		while ( (f >>= 1) != 0 && --shift ) { }
	}
	bass_shift_ = shift;
}

void Blip_Buffer::end_frame( blip_time_t t )
{
	offset_ += t * factor_;
	assert( samples_avail() <= (int) buffer_size_ ); // fails if time is past end of buffer
}

int Blip_Buffer::count_samples( blip_time_t t ) const
{
	blip_resampled_time_t last_sample  = resampled_time( t ) >> BLIP_BUFFER_ACCURACY;
	blip_resampled_time_t first_sample = offset_             >> BLIP_BUFFER_ACCURACY;
	return (int) (last_sample - first_sample);
}

blip_time_t Blip_Buffer::count_clocks( int count ) const
{
	if ( count > buffer_size_ )
		count = buffer_size_;
	blip_resampled_time_t time = (blip_resampled_time_t) count << BLIP_BUFFER_ACCURACY;
	return (blip_time_t) ((time - offset_ + factor_ - 1) / factor_);
}

void Blip_Buffer::remove_samples( int count )
{
	if ( count )
	{
		remove_silence( count );
		
		// copy remaining samples to beginning and clear old samples
		int remain = samples_avail() + blip_buffer_extra_;
		memmove( buffer_, buffer_ + count, remain * sizeof *buffer_ );
		memset( buffer_ + remain, 0, count * sizeof *buffer_ );
	}
}

int Blip_Buffer::read_samples( blip_sample_t out_ [], int max_samples, bool stereo )
{
	int count = samples_avail();
	if ( count > max_samples )
		count = max_samples;
	
	if ( count )
	{
		int const bass = highpass_shift();
		delta_t const* reader = read_pos() + count;
		int reader_sum = integrator();
		
		blip_sample_t* BLARGG_RESTRICT out = out_ + count;
		if ( stereo )
			out += count;
		int offset = -count;
		
		if ( !stereo )
		{
			do
			{
				int s = reader_sum >> delta_bits;
				
				reader_sum -= reader_sum >> bass;
				reader_sum += reader [offset];
				
				BLIP_CLAMP( s, s );
				out [offset] = (blip_sample_t) s;
			}
			while ( ++offset );
		}
		else
		{
			do
			{
				int s = reader_sum >> delta_bits;
				
				reader_sum -= reader_sum >> bass;
				reader_sum += reader [offset];
				
				BLIP_CLAMP( s, s );
				out [offset * 2] = (blip_sample_t) s;
			}
			while ( ++offset );
		}
		
		set_integrator( reader_sum );
		
		remove_samples( count );
	}
	return count;
}

void Blip_Buffer::mix_samples( blip_sample_t const in [], int count )
{
	delta_t* out = buffer_center_ + (offset_ >> BLIP_BUFFER_ACCURACY);
	
	int const sample_shift = blip_sample_bits - 16;
	int prev = 0;
	while ( --count >= 0 )
	{
		int s = *in++ << sample_shift;
		*out += s - prev;
		prev = s;
		++out;
	}
	*out -= prev;
}

void Blip_Buffer::save_state( blip_buffer_state_t* out )
{
	assert( samples_avail() == 0 );
	out->offset_       = offset_;
	out->reader_accum_ = reader_accum_;
	memcpy( out->buf, &buffer_ [offset_ >> BLIP_BUFFER_ACCURACY], sizeof out->buf );
}

void Blip_Buffer::load_state( blip_buffer_state_t const& in )
{
	clear();
	
	offset_       = in.offset_;
	reader_accum_ = in.reader_accum_;
	memcpy( buffer_, in.buf, sizeof in.buf );
}


//// Blip_Synth_

Blip_Synth_Fast_::Blip_Synth_Fast_()
{
	buf          = NULL;
	last_amp     = 0;
	delta_factor = 0;
}

void Blip_Synth_Fast_::volume_unit( double new_unit )
{
	delta_factor = int (new_unit * (1 << blip_sample_bits) + 0.5);
}

#if BLIP_BUFFER_FAST

void blip_eq_t::generate( float* out, int count ) const { }

#else

Blip_Synth_::Blip_Synth_( short p [], int w ) :
	phases( p ),
	width( w )
{
	volume_unit_ = 0.0;
	kernel_unit  = 0;
	buf          = NULL;
	last_amp     = 0;
	delta_factor = 0;
}

#undef PI
#define PI 3.1415926535897932384626433832795029

// Generates right half of sinc kernel (including center point) with cutoff at
// sample rate / 2 / oversample. Frequency response at cutoff frequency is
// treble dB (-6=0.5,-12=0.25). Mid controls frequency that rolloff begins at,
// cut * sample rate / 2.
static void gen_sinc( float out [], int out_size, double oversample,
		double treble, double mid )
{
	if ( mid    > 0.9999 ) mid    = 0.9999;
	if ( treble < -300.0 ) treble = -300.0;
	if ( treble >    5.0 ) treble =    5.0;
	
	double const maxh = 4096.0;
	double rolloff = pow( 10.0, 1.0 / (maxh * 20.0) * treble / (1.0 - mid) );
	double const pow_a_n = pow( rolloff, maxh - maxh * mid );
	double const to_angle = PI / maxh / oversample;
	for ( int i = 1; i < out_size; i++ )
	{
		double angle = i * to_angle;
		double c = rolloff *   cos( angle * maxh       - angle ) -
		                       cos( angle * maxh               );
		double cos_nc_angle  = cos( angle * maxh * mid         );
		double cos_nc1_angle = cos( angle * maxh * mid - angle );
		double cos_angle     = cos( angle                         );
		
		c = c * pow_a_n - rolloff * cos_nc1_angle + cos_nc_angle;
		double d = 1.0 + rolloff * (rolloff - cos_angle - cos_angle);
		double b = 2.0 - cos_angle - cos_angle;
		double a = 1.0 - cos_angle - cos_nc_angle + cos_nc1_angle;
		
		out [i] = (float) ((a * d + c * b) / (b * d)); // a / b + c / d
	}
	
	// Approximate center by looking at two points to right. Much simpler
	// and more reliable than trying to calculate it properly.
	out [0] = out [1] + 0.5 * (out [1] - out [2]);
}

// Gain is 1-2800 for beta of 0-10, instead of 1.0 as it should be, but
// this is corrected by normalization in treble_eq().
static void kaiser_window( float io [], int count, float beta )
{
	int const accuracy = 10;
	
	float const beta2 = beta * beta;
	float const step = (float) 0.5 / count;
	float pos = (float) 0.5;
	for ( float* const end = io + count; io < end; ++io )
	{
		float x = (pos - pos*pos) * beta2;
		float u = x;
		float k = 1;
		float n = 2;
		
		// Keep refining until adjustment becomes small
		do
		{
			u *= x / (n * n);
			n += 1;
			k += u;
		}
		while ( k <= u * (1 << accuracy) );
		
		pos += step;
		*io *= k;
	}
}

void blip_eq_t::generate( float out [], int count ) const
{
	// lower cutoff freq for narrow kernels with their wider transition band
	// (8 points->1.49, 16 points->1.15)
	double cutoff_adj = blip_res * 2.25 / count + 0.85;
	if ( cutoff_adj < 1.02 )
		cutoff_adj = 1.02;
	double half_rate = sample_rate * 0.5;
	if ( cutoff_freq )
		cutoff_adj = half_rate / cutoff_freq;
	double cutoff = rolloff_freq * cutoff_adj / half_rate;
	
	gen_sinc( out, count, oversample * cutoff_adj, treble, cutoff );
	
	kaiser_window( out, count, kaiser );
}

void Blip_Synth_::treble_eq( blip_eq_t const& eq )
{
	// Generate right half of kernel
	int const half_size = blip_eq_t::calc_count( width );
	float fimpulse [blip_res / 2 * (BLIP_MAX_QUALITY - 1) + 1];
	eq.generate( fimpulse, half_size );
	
	int i;
	
	// Find rescale factor. Summing from small to large (right to left)
	// reduces error.
	double total = 0.0;
	for ( i = half_size; --i > 0; )
		total += fimpulse [i];
	total = total * 2.0 + fimpulse [0];
	
	//double const base_unit = 44800.0 - 128 * 18; // allows treble up to +0 dB
	//double const base_unit = 37888.0; // allows treble to +5 dB
	double const base_unit = 32768.0; // necessary for blip_unscaled to work
	double rescale = base_unit / total;
	kernel_unit = (int) base_unit;
	
	// Integrate, first difference, rescale, convert to int
	double sum  = 0;
	double next = 0;
	int const size = impulses_size();
	for ( i = 0; i < size; i++ )
	{
		int j = (half_size - 1) - i;
		
		if ( i >= blip_res )
			sum += fimpulse [j + blip_res];
		
		// goes slightly past center, so it needs a little mirroring
		next += fimpulse [j < 0 ? -j : j];
		
		// calculate unintereleved index
		int x = (~i & (blip_res - 1)) * (width >> 1) + (i >> BLIP_PHASE_BITS);
		assert( (unsigned) x < (unsigned) size );
		
		// flooring separately virtually eliminates error
		phases [x] = (short) (int)
				(floor( sum * rescale + 0.5 ) - floor( next * rescale + 0.5 ));
		//phases [x] = (short) (int)
		//      floor( sum * rescale - next * rescale + 0.5 );
	}
	
	adjust_impulse();
	
	// volume might require rescaling
	double vol = volume_unit_;
	if ( vol )
	{
		volume_unit_ = 0.0;
		volume_unit( vol );
	}
}

void Blip_Synth_::adjust_impulse()
{
	int const size = impulses_size();
	int const half_width = width / 2;
	
	// Sum each phase as would be done when synthesizing, and correct
	// any that don't add up to exactly kernel_half.
	for ( int phase = blip_res / 2; --phase >= 0; )
	{
		int const fwd = phase * half_width;
		int const rev = size - half_width - fwd;
		
		int error = kernel_unit;
		for ( int i = half_width; --i >= 0; )
		{
			error += phases [fwd + i];
			error += phases [rev + i];
		}
		phases [fwd + half_width - 1] -= (short) error;
		
		// Error shouldn't occur now with improved calculation
		//if ( error ) printf( "error: %ld\n", error );
	}
	
	#if 0
		for ( int i = 0; i < blip_res; i++, printf( "\n" ) )
			for ( int j = 0; j < width / 2; j++ )
				printf( "%5d,", (int) -phases [j + width/2 * i] );
	#endif
}

void Blip_Synth_::rescale_kernel( int shift )
{
	// Keep values positive to avoid round-towards-zero of sign-preserving
	// right shift for negative values.
	int const keep_positive = 0x8000 + (1 << (shift - 1));
	
	int const half_width = width / 2;
	for ( int phase = blip_res; --phase >= 0; )
	{
		int const fwd = phase * half_width;
		
		// Integrate, rescale, then differentiate again.
		// If differences are rescaled directly, more error results.
		int sum = keep_positive;
		for ( int i = 0; i < half_width; i++ )
		{
			int prev = sum;
			sum += phases [fwd + i];
			phases [fwd + i] = (sum >> shift) - (prev >> shift);
		}
	}
	
	adjust_impulse();
}

void Blip_Synth_::volume_unit( double new_unit )
{
	if ( volume_unit_ != new_unit )
	{
		// use default eq if it hasn't been set yet
		if ( !kernel_unit )
			treble_eq( -8.0 );
		
		// Factor that kernel must be multiplied by
		volume_unit_ = new_unit;
		double factor = new_unit * (1 << blip_sample_bits) / kernel_unit;
		
		if ( factor > 0.0 )
		{
			// If factor is low, reduce amplitude of kernel itself
			int shift = 0;
			while ( factor < 2.0 )
			{
				shift++;
				factor *= 2.0;
			}
			
			if ( shift )
			{
				kernel_unit >>= shift;
				assert( kernel_unit > 0 ); // fails if volume unit is too low
				
				rescale_kernel( shift );
			}
		}
		
		delta_factor = -(int) floor( factor + 0.5 );
		//printf( "delta_factor: %d, kernel_unit: %d\n", delta_factor, kernel_unit );
	}
}
#endif
