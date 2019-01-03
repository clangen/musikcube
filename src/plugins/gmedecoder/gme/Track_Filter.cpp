// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Track_Filter.h"

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

int const fade_block_size = 512;
int const fade_shift = 8; // fade ends with gain at 1.0 / (1 << fade_shift)
int const silence_threshold = 8;

blargg_err_t Track_Filter::init( callbacks_t* c )
{
	callbacks = c;
	return buf.resize( buf_size );
}

void Track_Filter::clear_time_vars()
{
	emu_time      = buf_remain;
	out_time      = 0;
	silence_time  = 0;
	silence_count = 0;
}

void Track_Filter::stop()
{
	emu_track_ended_ = true;
	track_ended_     = true;
	fade_start       = indefinite_count;
	fade_step        = 1;
	buf_remain       = 0;
	emu_error        = NULL;
	clear_time_vars();
}

Track_Filter::Track_Filter() : setup_()
{
	callbacks          = NULL;
	setup_.max_silence = indefinite_count;
	silence_ignored_   = false;
	stop();
}

Track_Filter::~Track_Filter() { }

blargg_err_t Track_Filter::start_track()
{
	emu_error = NULL;
	stop();
	
	emu_track_ended_ = false;
	track_ended_     = false;
	
	if ( !silence_ignored_ )
	{
		// play until non-silence or end of track
		while ( emu_time < setup_.max_initial )
		{
			fill_buf();
			if ( buf_remain | emu_track_ended_ )
				break;
		}
	}
	
	clear_time_vars();
	return emu_error;
}

void Track_Filter::end_track_if_error( blargg_err_t err )
{
	if ( err )
	{
		emu_error        = err;
		emu_track_ended_ = true;
	}
}

blargg_err_t Track_Filter::skip( int count )
{
	emu_error = NULL;
	out_time += count;
	
	// remove from silence and buf first
	{
		int n = min( count, silence_count );
		silence_count -= n;
		count         -= n;
		
		n = min( count, buf_remain );
		buf_remain -= n;
		count      -= n;
	}
		
	if ( count && !emu_track_ended_ )
	{
		emu_time += count;
		silence_time = emu_time; // would otherwise be invalid
		end_track_if_error( callbacks->skip_( count ) );
	}
	
	if ( !(silence_count | buf_remain) ) // caught up to emulator, so update track ended
		track_ended_ |= emu_track_ended_;
	
	return emu_error;
}

blargg_err_t Track_Filter::skip_( int count )
{
	while ( count && !emu_track_ended_ )
	{
		int n = buf_size;
		if ( n > count )
			n = count;
		count -= n;
		RETURN_ERR( callbacks->play_( n, buf.begin() ) );
	}
	return blargg_ok;
}

// Fading

void Track_Filter::set_fade( int start, int length )
{
	fade_start = start;
	fade_step  = length / (fade_block_size * fade_shift);
	if ( fade_step < 1 )
		fade_step = 1;
}

bool Track_Filter::is_fading() const
{
	return out_time >= fade_start && fade_start != indefinite_count;
}

// unit / pow( 2.0, (double) x / step )
static int int_log( int x, int step, int unit )
{
	int shift = x / step;
	int fraction = (x - shift * step) * unit / step;
	return ((unit - fraction) + (fraction >> 1)) >> shift;
}

void Track_Filter::handle_fade( sample_t out [], int out_count )
{
	for ( int i = 0; i < out_count; i += fade_block_size )
	{
		int const shift = 14;
		int const unit = 1 << shift;
		int gain = int_log( (out_time + i - fade_start) / fade_block_size,
				fade_step, unit );
		if ( gain < (unit >> fade_shift) )
			track_ended_ = emu_track_ended_ = true;
		
		sample_t* io = &out [i];
		for ( int count = min( fade_block_size, out_count - i ); count; --count )
		{
			*io = sample_t ((*io * gain) >> shift);
			++io;
		}
	}
}

// Silence detection

void Track_Filter::emu_play( sample_t out [], int count )
{
	emu_time += count;
	if ( !emu_track_ended_ )
		end_track_if_error( callbacks->play_( count, out ) );
	else
		memset( out, 0, count * sizeof *out );
}

// number of consecutive silent samples at end
static int count_silence( Track_Filter::sample_t begin [], int size )
{
	Track_Filter::sample_t first = *begin;
	*begin = silence_threshold * 2; // sentinel
	Track_Filter::sample_t* p = begin + size;
	while ( (unsigned) (*--p + silence_threshold) <= (unsigned) silence_threshold * 2 ) { }
	*begin = first;
	return size - (p - begin);
}

// fill internal buffer and check it for silence
void Track_Filter::fill_buf()
{
	assert( !buf_remain );
	if ( !emu_track_ended_ )
	{
		emu_play( buf.begin(), buf_size );
		int silence = count_silence( buf.begin(), buf_size );
		if ( silence < buf_size )
		{
			silence_time = emu_time - silence;
			buf_remain   = buf_size;
			return;
		}
	}
	silence_count += buf_size;
}

blargg_err_t Track_Filter::play( int out_count, sample_t out [] )
{
	emu_error = NULL;
	if ( track_ended_ )
	{
		memset( out, 0, out_count * sizeof *out );
	}
	else
	{
		assert( emu_time >= out_time );
		
		// prints nifty graph of how far ahead we are when searching for silence
		//dprintf( "%*s \n", int ((emu_time - out_time) * 7 / 44100), "*" );
		
		// use any remaining silence samples
		int pos = 0;
		if ( silence_count )
		{
			if ( !silence_ignored_ )
			{
				// during a run of silence, run emulator at >=2x speed so it gets ahead
				int ahead_time = setup_.lookahead * (out_time + out_count - silence_time) +
						silence_time;
				while ( emu_time < ahead_time && !(buf_remain | emu_track_ended_) )
					fill_buf();
				
				// end track if sufficient silence has been found
				if ( emu_time - silence_time > setup_.max_silence )
				{
					track_ended_  = emu_track_ended_ = true;
					silence_count = out_count;
					buf_remain    = 0;
				}
			}
			
			// fill from remaining silence
			pos = min( silence_count, out_count );
			memset( out, 0, pos * sizeof *out );
			silence_count -= pos;
		}
		
		// use any remaining samples from buffer
		if ( buf_remain )
		{
			int n = min( buf_remain, (int) (out_count - pos) );
			memcpy( out + pos, buf.begin() + (buf_size - buf_remain), n * sizeof *out );
			buf_remain -= n;
			pos += n;
		}
		
		// generate remaining samples normally
		int remain = out_count - pos;
		if ( remain )
		{
			emu_play( out + pos, remain );
			track_ended_ |= emu_track_ended_;
			
			if ( silence_ignored_ && !is_fading() )
			{
				// if left unupdated, ahead_time could become too large
				silence_time = emu_time;
			}
			else
			{
				// check end for a new run of silence
				int silence = count_silence( out + pos, remain );
				if ( silence < remain )
					silence_time = emu_time - silence;
				
				if ( emu_time - silence_time >= buf_size )
					fill_buf(); // cause silence detection on next play()
			}
		}
		
		if ( is_fading() )
			handle_fade( out, out_count );
	}
	out_time += out_count;
	return emu_error;
}
