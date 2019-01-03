// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Music_Emu.h"

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

int const stereo = 2; // number of channels for stereo

Music_Emu::equalizer_t const Music_Emu::tv_eq = { -8.0, 180, 0,0,0,0,0,0,0,0 };

void Music_Emu::clear_track_vars()
{
	current_track_ = -1;
	warning(); // clear warning
	track_filter.stop();
}

void Music_Emu::unload()
{
	voice_count_ = 0;
	clear_track_vars();
	Gme_File::unload();
}

Music_Emu::gme_t()
{
	effects_buffer_ = NULL;
	sample_rate_    = 0;
	mute_mask_      = 0;
	tempo_          = 1.0;
	gain_           = 1.0;
    
    fade_set        = false;
	
	// defaults
	tfilter = track_filter.setup();
	set_max_initial_silence( 15 );
	set_silence_lookahead( 3 );
	ignore_silence( false );
	
	equalizer_.treble = -1.0;
	equalizer_.bass   = 60;
	
	static const char* const names [] = {
		"Voice 1", "Voice 2", "Voice 3", "Voice 4",
		"Voice 5", "Voice 6", "Voice 7", "Voice 8"
	};
	set_voice_names( names );
	Music_Emu::unload(); // clears fields
}

Music_Emu::~gme_t()
{
	assert( !effects_buffer_ );
}

blargg_err_t Music_Emu::set_sample_rate( int rate )
{
	require( !sample_rate() ); // sample rate can't be changed once set
	RETURN_ERR( set_sample_rate_( rate ) );
	RETURN_ERR( track_filter.init( this ) );
	sample_rate_ = rate;
	tfilter.max_silence = 6 * stereo * sample_rate();
	return blargg_ok;
}

void Music_Emu::pre_load()
{
	require( sample_rate() ); // set_sample_rate() must be called before loading a file
	Gme_File::pre_load();
}

void Music_Emu::set_equalizer( equalizer_t const& eq )
{
	// TODO: why is GCC generating memcpy call here?
	// Without the 'if', valgrind flags it.
	if ( &eq != &equalizer_ )
		equalizer_ = eq;
	set_equalizer_( eq );
}

void Music_Emu::mute_voice( int index, bool mute )
{
	require( (unsigned) index < (unsigned) voice_count() );
	int bit = 1 << index;
	int mask = mute_mask_ | bit;
	if ( !mute )
		mask ^= bit;
	mute_voices( mask );
}

void Music_Emu::mute_voices( int mask )
{
	require( sample_rate() ); // sample rate must be set first
	mute_mask_ = mask;
	mute_voices_( mask );
}

const char* Music_Emu::voice_name( int i ) const
{
	if ( (unsigned) i < (unsigned) voice_count_ )
		return voice_names_ [i];
	
	//check( false ); // TODO: enable?
	return "";
}

void Music_Emu::set_tempo( double t )
{
	require( sample_rate() ); // sample rate must be set first
	double const min = 0.02;
	double const max = 4.00;
	if ( t < min ) t = min;
	if ( t > max ) t = max;
	tempo_ = t;
	set_tempo_( t );
}

blargg_err_t Music_Emu::post_load()
{
	set_tempo( tempo_ );
	remute_voices();
	return Gme_File::post_load();
}

// Tell/Seek

int Music_Emu::msec_to_samples( int msec ) const
{
	int sec = msec / 1000;
	msec -= sec * 1000;
	return (sec * sample_rate() + msec * sample_rate() / 1000) * stereo;
}

int Music_Emu::tell() const
{
	int rate = sample_rate() * stereo;
	int sec = track_filter.sample_count() / rate;
	return sec * 1000 + (track_filter.sample_count() - sec * rate) * 1000 / rate;
}

blargg_err_t Music_Emu::seek( int msec )
{
	int time = msec_to_samples( msec );
	if ( time < track_filter.sample_count() )
    {
		RETURN_ERR( start_track( current_track_ ) );
        if ( fade_set )
            set_fade( length_msec, fade_msec );
    }
	return skip( time - track_filter.sample_count() );
}

blargg_err_t Music_Emu::skip( int count )
{
	require( current_track() >= 0 ); // start_track() must have been called already
	return track_filter.skip( count );
}

blargg_err_t Music_Emu::skip_( int count )
{
	// for long skip, mute sound
	const int threshold = 32768;
	if ( count > threshold )
	{
		int saved_mute = mute_mask_;
		mute_voices( ~0 );
		
		int n = count - threshold/2;
		n &= ~(2048-1); // round to multiple of 2048
		count -= n;
		RETURN_ERR( track_filter.skip_( n ) );
		
		mute_voices( saved_mute );
	}
	
	return track_filter.skip_( count );
}

// Playback

blargg_err_t Music_Emu::start_track( int track )
{
	clear_track_vars();
	
	int remapped = track;
	RETURN_ERR( remap_track_( &remapped ) );
	current_track_ = track;
	blargg_err_t err = start_track_( remapped );
	if ( err )
	{
		current_track_ = -1;
		return err;
	}
	
	// convert filter times to samples
	Track_Filter::setup_t s = tfilter;
	s.max_initial *= sample_rate() * stereo;
	#if GME_DISABLE_SILENCE_LOOKAHEAD
		s.lookahead = 1;
	#endif
	track_filter.setup( s );
	
	return track_filter.start_track();
}

void Music_Emu::set_fade( int start_msec, int length_msec )
{
    fade_set = true;
    this->length_msec = start_msec;
    this->fade_msec = length_msec;
	track_filter.set_fade( start_msec < 0 ? Track_Filter::indefinite_count : msec_to_samples( start_msec ),
			length_msec * sample_rate() / (1000 / stereo) );
}

blargg_err_t Music_Emu::play( int out_count, sample_t out [] )
{
	require( current_track() >= 0 );
	require( out_count % stereo == 0 );
	
	return track_filter.play( out_count, out );
}

// Gme_Info_

blargg_err_t Gme_Info_::set_sample_rate_( int )             { return blargg_ok; }
void         Gme_Info_::pre_load()                          { Gme_File::pre_load(); } // skip Music_Emu
blargg_err_t Gme_Info_::post_load()                         { return Gme_File::post_load(); } // skip Music_Emu
void         Gme_Info_::set_equalizer_( equalizer_t const& ){ check( false ); }
void         Gme_Info_::mute_voices_( int )                 { check( false ); }
void         Gme_Info_::set_tempo_( double )                { }
blargg_err_t Gme_Info_::start_track_( int )                 { return BLARGG_ERR( BLARGG_ERR_CALLER, "can't play file opened for info only" ); }
blargg_err_t Gme_Info_::play_( int, sample_t [] )           { return BLARGG_ERR( BLARGG_ERR_CALLER, "can't play file opened for info only" ); }
