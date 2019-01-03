// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Classic_Emu.h"

#include "Multi_Buffer.h"

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

Classic_Emu::Classic_Emu()
{
	buf           = NULL;
	stereo_buffer = NULL;
	voice_types   = NULL;
	
	// avoid inconsistency in our duplicated constants
	assert( (int) wave_type  == (int) Multi_Buffer::wave_type );
	assert( (int) noise_type == (int) Multi_Buffer::noise_type );
	assert( (int) mixed_type == (int) Multi_Buffer::mixed_type );
}

Classic_Emu::~Classic_Emu()
{
	delete stereo_buffer;
	delete effects_buffer_;
	effects_buffer_ = NULL;
}

void Classic_Emu::set_equalizer_( equalizer_t const& eq )
{
	Music_Emu::set_equalizer_( eq );
	update_eq( eq.treble );
	if ( buf )
		buf->bass_freq( (int) equalizer().bass );
}
	
blargg_err_t Classic_Emu::set_sample_rate_( int rate )
{
	if ( !buf )
	{
		if ( !stereo_buffer )
			CHECK_ALLOC( stereo_buffer = BLARGG_NEW Stereo_Buffer );
		buf = stereo_buffer;
	}
	return buf->set_sample_rate( rate, 1000 / 20 );
}

void Classic_Emu::mute_voices_( int mask )
{
	Music_Emu::mute_voices_( mask );
	for ( int i = voice_count(); i--; )
	{
		if ( mask & (1 << i) )
		{
			set_voice( i, NULL, NULL, NULL );
		}
		else
		{
			Multi_Buffer::channel_t ch = buf->channel( i );
			assert( (ch.center && ch.left && ch.right) ||
					(!ch.center && !ch.left && !ch.right) ); // all or nothing
			set_voice( i, ch.center, ch.left, ch.right );
		}
	}
}

void Classic_Emu::change_clock_rate( int rate )
{
	clock_rate_ = rate;
	buf->clock_rate( rate );
}

blargg_err_t Classic_Emu::setup_buffer( int rate )
{
	change_clock_rate( rate );
	RETURN_ERR( buf->set_channel_count( voice_count(), voice_types ) );
	set_equalizer( equalizer() );
	buf_changed_count = buf->channels_changed_count();
	return blargg_ok;
}

blargg_err_t Classic_Emu::start_track_( int track )
{
	RETURN_ERR( Music_Emu::start_track_( track ) );
	buf->clear();
	return blargg_ok;
}

blargg_err_t Classic_Emu::play_( int count, sample_t out [] )
{
	// read from buffer, then refill buffer and repeat if necessary
	int remain = count;
	while ( remain )
	{
		buf->disable_immediate_removal();
		remain -= buf->read_samples( &out [count - remain], remain );
		if ( remain )
		{
			if ( buf_changed_count != buf->channels_changed_count() )
			{
				buf_changed_count = buf->channels_changed_count();
				remute_voices();
			}
			
			// TODO: use more accurate length calculation
			int msec = buf->length();
			blip_time_t clocks_emulated = msec * clock_rate_ / 1000 - 100;
			RETURN_ERR( run_clocks( clocks_emulated, msec ) );
			assert( clocks_emulated );
			buf->end_frame( clocks_emulated );
		}
	}
	return blargg_ok;
}
