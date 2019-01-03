// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sap_Emu.h"

#include "blargg_endian.h"

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

Sap_Emu::Sap_Emu()
{
	set_type( gme_sap_type );
	set_silence_lookahead( 6 );
}

Sap_Emu::~Sap_Emu() { }

// Track info

// Returns 16 or greater if not hex. Handles uppercase and lowercase.
// Thoroughly tested and rejects ALL non-hex characters.
inline int from_hex_char( int h )
{
	h -= 0x30;
	if ( (unsigned) h > 9 )
		h = ((h - 0x11) & 0xDF) + 10;
	return h;
}

static int from_hex( byte const in [] )
{
	int result = 0;
	for ( int n = 4; n--; )
	{
		int h = from_hex_char( *in++ );
		if ( h > 15 )
			return -1;
		result = result * 0x10 + h;
	}
	return result;
}

static int parse_int( byte const* io [], byte const* end )
{
	byte const* in = *io;
	int n = 0;
	while ( in < end )
	{
		int dig = *in - '0';
		if ( (unsigned) dig > 9 )
			break;
		++in;
		n = n * 10 + dig;
	}
	if ( in == *io )
		n = -1; // no numeric characters
	*io = in;
	return n;
}

static int from_dec( byte const in [], byte const* end )
{
	int n = parse_int( &in, end );
	if ( in < end )
		n = -1;
	return n;
}

static void parse_string( byte const in [], byte const* end, int len, char out [] )
{
	byte const* start = in;
	if ( *in++ == '\"' )
	{
		start++;
		while ( in < end && *in != '\"' )
			in++;
	}
	else
	{
		in = end;
	}
	len = min( len - 1, int (in - start) );
	out [len] = 0;
	memcpy( out, start, len );
}

static int parse_time( byte const in [], byte const* end )
{
	int minutes = parse_int( &in, end );
	if ( minutes < 0 || *in != ':' )
		return 0;
	
	++in;
	int seconds = parse_int( &in, end );
	if ( seconds < 0 )
		return 0;
	
	int time = minutes * 60000 + seconds * 1000;
	if ( *in == '.' )
	{
		byte const* start = ++in;
		int msec = parse_int( &in, end );
		if ( msec >= 0 )
		{
			// allow 1-3 digits
			for ( int n = in - start; n < 3; n++ )
				msec *= 10;
			time += msec;
		}
	}
	
	while ( in < end && *in <= ' ' )
		++in;
	
	if ( end - in >= 4 && !memcmp( in, "LOOP", 4 ) )
		time = -time;
	
	return time;
}

static blargg_err_t parse_info( byte const in [], int size, Sap_Emu::info_t* out )
{
	out->track_count   = 1;
	out->author    [0] = 0;
	out->name      [0] = 0;
	out->copyright [0] = 0;
	
	for ( int i = 0; i < Sap_Emu::max_tracks; i++ )
		out->track_times [i] = 0;
	
	if ( size < 16 || memcmp( in, "SAP\x0D\x0A", 5 ) )
		return blargg_err_file_type;
	
	int time_count = 0;
	byte const* file_end = in + size - 5;
	in += 5;
	while ( in < file_end && (in [0] != 0xFF || in [1] != 0xFF) )
	{
		byte const* line_end = in;
		while ( line_end < file_end && *line_end != 0x0D )
			line_end++;
		
		char const* tag = (char const*) in;
		while ( in < line_end && *in > ' ' )
			in++;
		int tag_len = (char const*) in - tag;
		
		while ( in < line_end && *in <= ' ' ) in++;
		
		if ( tag_len <= 0 )
		{
			// skip line
		}
		else if ( !strncmp( "TIME", tag, tag_len ) && time_count < Sap_Emu::max_tracks )
		{
			out->track_times [time_count++] = parse_time( in, line_end );
		}
		else if ( !strncmp( "INIT", tag, tag_len ) )
		{
			out->init_addr = from_hex( in );
			if ( (unsigned) out->init_addr >= 0x10000 )
				return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "init address" );
		}
		else if ( !strncmp( "PLAYER", tag, tag_len ) )
		{
			out->play_addr = from_hex( in );
			if ( (unsigned) out->play_addr >= 0x10000 )
				return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "play address" );
		}
		else if ( !strncmp( "MUSIC", tag, tag_len ) )
		{
			out->music_addr = from_hex( in );
			if ( (unsigned) out->music_addr >= 0x10000 )
				return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "music address" );
		}
		else if ( !strncmp( "SONGS", tag, tag_len ) )
		{
			out->track_count = from_dec( in, line_end );
			if ( out->track_count <= 0 )
				return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "track count" );
		}
		else if ( !strncmp( "TYPE", tag, tag_len ) )
		{
			switch ( out->type = *in )
			{
			case 'S':
				out->type = 'C';
			case 'B':
			case 'C':
			case 'D':
				break;
			
			default:
				return BLARGG_ERR( BLARGG_ERR_FILE_FEATURE, "player type" );
			}
		}
		else if ( !strncmp( "STEREO", tag, tag_len ) )
		{
			out->stereo = true;
		}
		else if ( !strncmp( "FASTPLAY", tag, tag_len ) )
		{
			out->fastplay = from_dec( in, line_end );
			if ( out->fastplay <= 0 )
				return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "fastplay value" );
		}
		else if ( !strncmp( "AUTHOR", tag, tag_len ) )
		{
			parse_string( in, line_end, sizeof out->author, out->author );
		}
		else if ( !strncmp( "NAME", tag, tag_len ) )
		{
			parse_string( in, line_end, sizeof out->name, out->name );
		}
		else if ( !strncmp( "DATE", tag, tag_len ) )
		{
			parse_string( in, line_end, sizeof out->copyright, out->copyright );
		}
		
		in = line_end + 2;
	}
	
	if ( in [0] != 0xFF || in [1] != 0xFF )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "ROM data missing" );
	out->rom_data = in + 2;
	
	return blargg_ok;
}

static void copy_sap_fields( Sap_Emu::info_t const& in, track_info_t* out )
{
	Gme_File::copy_field_( out->game,      in.name );
	Gme_File::copy_field_( out->author,    in.author );
	Gme_File::copy_field_( out->copyright, in.copyright );
}

static void hash_sap_file( Sap_Emu::info_t const& i, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	unsigned char temp[4];
	set_le32( &temp[0], i.init_addr ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.play_addr ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.music_addr ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.type ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.fastplay ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.stereo ); out.hash_( &temp[0], sizeof(temp) );
	set_le32( &temp[0], i.track_count ); out.hash_( &temp[0], sizeof(temp) );
	out.hash_( data, data_size );
}

blargg_err_t Sap_Emu::track_info_( track_info_t* out, int track ) const
{
	copy_sap_fields( info_, out );
	
	if ( track < max_tracks )
	{
		int time = info_.track_times [track];
		if ( time )
		{
			if ( time > 0 )
			{
				out->loop_length = 0;
			}
			else
			{
				time = -time;
				out->loop_length = time;
			}
			out->length = time;
		}
	}
	return blargg_ok;
}

struct Sap_File : Gme_Info_
{
	Sap_Emu::info_t info;
	
	Sap_File() { set_type( gme_sap_type ); }
	
	blargg_err_t load_mem_( byte const begin [], int size )
	{
		RETURN_ERR( parse_info( begin, size, &info ) );
		set_track_count( info.track_count );
		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int track ) const
	{
		copy_sap_fields( info, out );

		if (track < Sap_Emu::max_tracks)
		{
			int time = info.track_times[track];
			if (time)
			{
				if (time > 0)
				{
					out->loop_length = 0;
				}
				else
				{
					time = -time;
					out->loop_length = time;
				}
				out->length = time;
			}
		}

		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_sap_file( info, info.rom_data, file_end() - info.rom_data, out );
		return blargg_ok;
	}
};

static Music_Emu* new_sap_emu () { return BLARGG_NEW Sap_Emu ; }
static Music_Emu* new_sap_file() { return BLARGG_NEW Sap_File; }

gme_type_t_ const gme_sap_type [1] = {{ "Atari XL", 0, &new_sap_emu, &new_sap_file, "SAP", 1 }};

// Setup

blargg_err_t Sap_Emu::load_mem_( byte const in [], int size )
{
	file_end = in + size;
	
	info_.warning    = NULL;
	info_.type       = 'B';
	info_.stereo     = false;
	info_.init_addr  = -1;
	info_.play_addr  = -1;
	info_.music_addr = -1;
	info_.fastplay   = 312;
	RETURN_ERR( parse_info( in, size, &info_ ) );
	
	set_warning( info_.warning );
	set_track_count( info_.track_count );
	set_voice_count( Sap_Apu::osc_count << info_.stereo );
	core.apu_impl().volume( gain() );
	
	static const char* const names [Sap_Apu::osc_count * 2] = {
		"Wave 1", "Wave 2", "Wave 3", "Wave 4",
		"Wave 5", "Wave 6", "Wave 7", "Wave 8",
	};
	set_voice_names( names );
	
	static int const types [Sap_Apu::osc_count * 2] = {
		wave_type+1, wave_type+2, wave_type+3, wave_type+0,
		wave_type+5, wave_type+6, wave_type+7, wave_type+4,
	};
	set_voice_types( types );
	
	return setup_buffer( 1773447 );
}

void Sap_Emu::update_eq( blip_eq_t const& eq )
{
	core.apu_impl().synth.treble_eq( eq );
}

void Sap_Emu::set_voice( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	int i2 = i - Sap_Apu::osc_count;
	if ( i2 >= 0 )
		core.apu2().set_output( i2, right );
	else
		core.apu().set_output( i, (info_.stereo ? left : center) );
}

// Emulation

void Sap_Emu::set_tempo_( double t )
{
	core.set_tempo( t );
}

blargg_err_t Sap_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	core.setup_ram();
	
	// Copy file data to RAM
	byte const* in = info_.rom_data;
	while ( file_end - in >= 5 )
	{
		int start = get_le16( in );
		int end   = get_le16( in + 2 );
		//dprintf( "Block $%04X-$%04X\n", start, end );
		in += 4;
		int len = end - start + 1;
		if ( (unsigned) len > (unsigned) (file_end - in) )
		{
			set_warning( "Invalid file data block" );
			break;
		}
		
		memcpy( core.ram() + start, in, len );
		in += len;
		if ( file_end - in >= 2 && in [0] == 0xFF && in [1] == 0xFF )
			in += 2;
	}
	
	return core.start_track( track, info_ );
}

blargg_err_t Sap_Emu::run_clocks( blip_time_t& duration, int )
{
	return core.end_frame( duration );
}

blargg_err_t Sap_Emu::hash_( Hash_Function& out ) const
{
	hash_sap_file( info(), info().rom_data, file_end - info().rom_data, out );
	return blargg_ok;
}
