// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gym_Emu.h"

#include "blargg_endian.h"

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

double const min_tempo  = 0.25;
double const oversample = 5 / 3.0;
double const fm_gain    = 3.0;

int const base_clock = 53700300;
int const clock_rate = base_clock / 15;

Gym_Emu::Gym_Emu()
{
	resampler.set_callback( play_frame_, this );
	pos = NULL;
	disable_oversampling_ = false;
	set_type( gme_gym_type );
	set_silence_lookahead( 1 ); // tracks should already be trimmed
	pcm_buf = stereo_buf.center();
}

Gym_Emu::~Gym_Emu() { }

// Track info

static void get_gym_info( Gym_Emu::header_t const& h, int length, track_info_t* out )
{
	if ( 0 != memcmp( h.tag, "GYMX", 4 ) )
		return;
	
	length = length * 50 / 3; // 1000 / 60
	int loop = get_le32( h.loop_start );
	if ( loop )
	{
		out->intro_length = loop * 50 / 3;
		out->loop_length  = length - out->intro_length;
	}
	else
	{
		out->length = length;
		out->intro_length = length; // make it clear that track is no longer than length
		out->loop_length = 0;
	}
	
	// more stupidity where the field should have been left blank
	if ( strcmp( h.song, "Unknown Song" ) )
		GME_COPY_FIELD( h, out, song );
	
	if ( strcmp( h.game, "Unknown Game" ) )
		GME_COPY_FIELD( h, out, game );
	
	if ( strcmp( h.copyright, "Unknown Publisher" ) )
		GME_COPY_FIELD( h, out, copyright );
	
	if ( strcmp( h.dumper, "Unknown Person" ) )
		GME_COPY_FIELD( h, out, dumper );
	
	if ( strcmp( h.comment, "Header added by YMAMP" ) )
		GME_COPY_FIELD( h, out, comment );
}

static void hash_gym_file( Gym_Emu::header_t const& h, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.loop_start[0], sizeof(h.loop_start) );
	out.hash_( &h.packed[0], sizeof(h.packed) );
	out.hash_( data, data_size );
}

static int gym_track_length( byte const p [], byte const* end )
{
	int time = 0;
	while ( p < end )
	{
		switch ( *p++ )
		{
		case 0:
			time++;
			break;
		
		case 1:
		case 2:
			p += 2;
			break;
		
		case 3:
			p += 1;
			break;
		}
	}
	return time;
}

blargg_err_t Gym_Emu::track_info_( track_info_t* out, int ) const
{
	get_gym_info( header_, gym_track_length( log_begin(), file_end() ), out );
	return blargg_ok;
}

static blargg_err_t check_header( byte const in [], int size, int* data_offset = NULL )
{
	if ( size < 4 )
		return blargg_err_file_type;
	
	if ( memcmp( in, "GYMX", 4 ) == 0 )
	{
		if ( size < Gym_Emu::header_t::size + 1 )
			return blargg_err_file_type;
		
		if ( memcmp( ((Gym_Emu::header_t const*) in)->packed, "\0\0\0\0", 4 ) != 0 )
			return BLARGG_ERR( BLARGG_ERR_FILE_FEATURE, "packed GYM file" );
		
		if ( data_offset )
			*data_offset = Gym_Emu::header_t::size;
	}
	else if ( *in > 3 )
	{
		return blargg_err_file_type;
	}
	
	return blargg_ok;
}

struct Gym_File : Gme_Info_
{
	int data_offset;
	
	Gym_File() { set_type( gme_gym_type ); }
	
	blargg_err_t load_mem_( byte const in [], int size )
	{
		data_offset = 0;
		return check_header( in, size, &data_offset );
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		int length = gym_track_length( &file_begin() [data_offset], file_end() );
		get_gym_info( *(Gym_Emu::header_t const*) file_begin(), length, out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		Gym_Emu::header_t const* h = ( Gym_Emu::header_t const* ) file_begin();
		byte const* data = &file_begin() [data_offset];

		hash_gym_file( *h, data, file_end() - data, out );

		return blargg_ok;
	}
};

static Music_Emu* new_gym_emu () { return BLARGG_NEW Gym_Emu ; }
static Music_Emu* new_gym_file() { return BLARGG_NEW Gym_File; }

gme_type_t_ const gme_gym_type [1] = {{ "Sega Genesis", 1, &new_gym_emu, &new_gym_file, "GYM", 0 }};

// Setup

blargg_err_t Gym_Emu::set_sample_rate_( int sample_rate )
{
	blip_eq_t eq( -32, 8000, sample_rate );
	apu.treble_eq( eq );
	pcm_synth.treble_eq( eq );
	
	apu.volume( 0.135 * fm_gain * gain() );
	
	double factor = oversample;
	if ( disable_oversampling_ )
		factor = (double) base_clock / 7 / 144 / sample_rate;
	RETURN_ERR( resampler.setup( factor, 0.990, fm_gain * gain() ) );
	factor = resampler.rate();
	double fm_rate = sample_rate * factor;
	
	RETURN_ERR( stereo_buf.set_sample_rate( sample_rate, int (1000 / 60.0 / min_tempo) ) );
	stereo_buf.clock_rate( clock_rate );
	
	RETURN_ERR( fm.set_rate( fm_rate, base_clock / 7.0 ) );
	RETURN_ERR( resampler.reset( (int) (1.0 / 60 / min_tempo * sample_rate) ) );
	
	return blargg_ok;
}

void Gym_Emu::set_tempo_( double t )
{
	if ( t < min_tempo )
	{
		set_tempo( min_tempo );
		return;
	}
	
	if ( stereo_buf.sample_rate() )
	{
		double denom = tempo() * 60;
		clocks_per_frame = (int) (clock_rate / denom);
		resampler.resize( (int) (sample_rate() / denom) );
	}
}

void Gym_Emu::mute_voices_( int mask )
{
	Music_Emu::mute_voices_( mask );
	fm.mute_voices( mask );
	apu.set_output( (mask & 0x80) ? 0 : stereo_buf.center() );
	pcm_synth.volume( (mask & 0x40) ? 0.0 : 0.125 / 256 * fm_gain * gain() );
}

blargg_err_t Gym_Emu::load_mem_( byte const in [], int size )
{
	assert( offsetof (header_t,packed [4]) == header_t::size );
	log_offset = 0;
	RETURN_ERR( check_header( in, size, &log_offset ) );
	
	loop_begin = NULL;
	
	static const char* const names [] = {
		"FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6", "PCM", "PSG"
	};
	set_voice_names( names );
	
	set_voice_count( 8 );
	
	if ( log_offset )
		header_ = *(header_t const*) in;
	else
		memset( &header_, 0, sizeof header_ );
	
	return blargg_ok;
}

// Emulation

blargg_err_t Gym_Emu::start_track_( int track )
{
	RETURN_ERR( Music_Emu::start_track_( track ) );
	
	pos         = log_begin();
	loop_remain = get_le32( header_.loop_start );
	
	prev_pcm_count = 0;
	pcm_enabled    = 0;
	pcm_amp        = -1;
	
	fm.reset();
	apu.reset();
	stereo_buf.clear();
	resampler.clear();
	pcm_buf = stereo_buf.center();
	return blargg_ok;
}

void Gym_Emu::run_pcm( byte const pcm_in [], int pcm_count )
{
	// Guess beginning and end of sample and adjust rate and buffer position accordingly.
	
	// count dac samples in next frame
	int next_pcm_count = 0;
	const byte* p = this->pos;
	int cmd;
	while ( (cmd = *p++) != 0 )
	{
		int data = *p++;
		if ( cmd <= 2 )
			++p;
		if ( cmd == 1 && data == 0x2A )
			next_pcm_count++;
	}
	
	// detect beginning and end of sample
	int rate_count = pcm_count;
	int start = 0;
	if ( !prev_pcm_count && next_pcm_count && pcm_count < next_pcm_count )
	{
		rate_count = next_pcm_count;
		start = next_pcm_count - pcm_count;
	}
	else if ( prev_pcm_count && !next_pcm_count && pcm_count < prev_pcm_count )
	{
		rate_count = prev_pcm_count;
	}
	
	// Evenly space samples within buffer section being used
	blip_resampled_time_t period = pcm_buf->resampled_duration( clocks_per_frame ) / rate_count;
	
	blip_resampled_time_t time = pcm_buf->resampled_time( 0 ) + period * start + (unsigned) period / 2;
	
	int pcm_amp = this->pcm_amp;
	if ( pcm_amp < 0 )
		pcm_amp = pcm_in [0];
	
	for ( int i = 0; i < pcm_count; i++ )
	{
		int delta = pcm_in [i] - pcm_amp;
		pcm_amp += delta;
		pcm_synth.offset_resampled( time, delta, pcm_buf );
		time += period;
	}
	this->pcm_amp = pcm_amp;
	pcm_buf->set_modified();
}

void Gym_Emu::parse_frame()
{
	byte pcm [1024]; // all PCM writes for frame
	int pcm_size = 0;
	const byte* pos = this->pos;
	
	if ( loop_remain && !--loop_remain )
		loop_begin = pos; // find loop on first time through sequence
	
	int cmd;
	while ( (cmd = *pos++) != 0 )
	{
		int data = *pos++;
		if ( cmd == 1 )
		{
			int data2 = *pos++;
			if ( data == 0x2A )
			{
				pcm [pcm_size] = data2;
				if ( pcm_size < (int) sizeof pcm - 1 )
					pcm_size += pcm_enabled;
			}
			else
			{
				if ( data == 0x2B )
					pcm_enabled = data2 >> 7 & 1;
				
				fm.write0( data, data2 );
			}
		}
		else if ( cmd == 2 )
		{
			int data2 = *pos++;
			if ( data == 0xB6 )
			{
				Blip_Buffer * pcm_buf = NULL;
				switch ( data2 >> 6 )
				{
				case 0: pcm_buf = NULL; break;
				case 1: pcm_buf = stereo_buf.right(); break;
				case 2: pcm_buf = stereo_buf.left(); break;
				case 3: pcm_buf = stereo_buf.center(); break;
				}
				/*if ( this->pcm_buf != pcm_buf )
				{
					if ( this->pcm_buf ) pcm_synth.offset_inline( 0, -pcm_amp, this->pcm_buf );
					if ( pcm_buf )       pcm_synth.offset_inline( 0,  pcm_amp, pcm_buf );
				}*/
				this->pcm_buf = pcm_buf;
			}
			fm.write1( data, data2 );
		}
		else if ( cmd == 3 )
		{
			apu.write_data( 0, data );
		}
		else
		{
			// to do: many GYM streams are full of errors, and error count should
			// reflect cases where music is really having problems
			//log_error(); 
			--pos; // put data back
		}
	}
	
	if ( pos >= file_end() )
	{
		// Reached end
		check( pos == file_end() );
		
		if ( loop_begin )
			pos = loop_begin;
		else
			set_track_ended();
	}
	this->pos = pos;
	
	// PCM
	if ( pcm_buf && pcm_size )
		run_pcm( pcm, pcm_size );
	prev_pcm_count = pcm_size;
}

inline int Gym_Emu::play_frame( blip_time_t blip_time, int sample_count, sample_t buf [] )
{
	if ( !track_ended() )
		parse_frame();
	
	apu.end_frame( blip_time );
	
	memset( buf, 0, sample_count * sizeof *buf );
	fm.run( sample_count >> 1, buf );
	
	return sample_count;
}

int Gym_Emu::play_frame_( void* p, blip_time_t a, int b, sample_t c [] )
{
	return STATIC_CAST(Gym_Emu*,p)->play_frame( a, b, c );
}

blargg_err_t Gym_Emu::play_( int count, sample_t out [] )
{
	resampler.dual_play( count, out, stereo_buf );
	return blargg_ok;
}

blargg_err_t Gym_Emu::hash_( Hash_Function& out ) const
{
	hash_gym_file( header(), log_begin(), file_end() - log_begin(), out );
	return blargg_ok;
}