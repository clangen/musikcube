// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Nsfe_Emu.h"

#include "blargg_endian.h"
#include <ctype.h>

/* Copyright (C) 2005-2009 Shay Green. This module is free software; you
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

Nsfe_Info::Nsfe_Info() { playlist_disabled = false; }

Nsfe_Info::~Nsfe_Info() { }

inline void Nsfe_Info::unload()
{
	data.clear();
	track_name_data.clear();
	track_names.clear();
	playlist.clear();
	track_times.clear();
}

// TODO: if no playlist, treat as if there is a playlist that is just 1,2,3,4,5... ?
void Nsfe_Info::disable_playlist( bool b )
{
	playlist_disabled = b;
	info.track_count = playlist.size();
	if ( !info.track_count || playlist_disabled )
		info.track_count = actual_track_count_;
}

int Nsfe_Info::remap_track( int track ) const
{
	if ( !playlist_disabled && (unsigned) track < playlist.size() )
		track = playlist [track];
	return track;
}

// Read multiple strings and separate into individual strings
static blargg_err_t read_strs( Data_Reader& in, int size, blargg_vector<char>& chars,
		blargg_vector<const char*>& strs )
{
	RETURN_ERR( chars.resize( size + 1 ) );
	chars [size] = 0; // in case last string doesn't have terminator
	RETURN_ERR( in.read( &chars [0], size ) );
	
	RETURN_ERR( strs.resize( 128 ) );
	int count = 0;
	for ( int i = 0; i < size; i++ )
	{
		if ( (int) strs.size() <= count )
			RETURN_ERR( strs.resize( count * 2 ) );
		strs [count++] = &chars [i];
		while ( i < size && chars [i] )
			i++;
	}
	
	return strs.resize( count );
}

// Copy in to out, where out has out_max characters allocated. Truncate to
// out_max - 1 characters.
static void copy_str( const char in [], char out [], int out_max )
{
	out [out_max - 1] = 0;
	strncpy( out, in, out_max - 1 );
}

struct nsfe_info_t
{
	byte load_addr [2];
	byte init_addr [2];
	byte play_addr [2];
	byte speed_flags;
	byte chip_flags;
	byte track_count;
	byte first_track;
	byte unused [6];
};

blargg_err_t Nsfe_Info::load( Data_Reader& in, Nsfe_Emu* nsf_emu )
{
	int const nsfe_info_size = 16;
	assert( offsetof (nsfe_info_t,unused [6]) == nsfe_info_size );
	
	// check header
	byte signature [4];
	blargg_err_t err = in.read( signature, sizeof signature );
	if ( err )
		return (blargg_is_err_type( err, blargg_err_file_eof ) ? blargg_err_file_type : err);
	if ( memcmp( signature, "NSFE", 4 ) )
		return blargg_err_file_type;
	
	// free previous info
	track_name_data.clear();
	track_names.clear();
	playlist.clear();
	track_times.clear();
	
	// default nsf header
	static const Nsf_Emu::header_t base_header =
	{
		{'N','E','S','M','\x1A'},// tag
		1,                  // version
		1, 1,               // track count, first track
		{0,0},{0,0},{0,0},  // addresses
		"","","",           // strings
		{0x1A, 0x41},       // NTSC rate
		{0,0,0,0,0,0,0,0},  // banks
		{0x20, 0x4E},       // PAL rate
		0, 0,               // flags
		{0,0,0,0}           // unused
	};
	Nsf_Emu::header_t& header = info;
	header = base_header;
	
	// parse tags
	int phase = 0;
	while ( phase != 3 )
	{
		// read size and tag
		byte block_header [2] [4];
		RETURN_ERR( in.read( block_header, sizeof block_header ) );
		int size = get_le32( block_header [0] );
		int tag  = get_le32( block_header [1] );
		
		//dprintf( "tag: %c%c%c%c\n", char(tag), char(tag>>8), char(tag>>16), char(tag>>24) );
		
		switch ( tag )
		{
			case BLARGG_4CHAR('O','F','N','I'): {
				check( phase == 0 );
				if ( size < 8 )
					return blargg_err_file_corrupt;
				
				nsfe_info_t finfo;
				finfo.track_count = 1;
				finfo.first_track = 0;
				
				RETURN_ERR( in.read( &finfo, min( size, (int) nsfe_info_size ) ) );
				if ( size > nsfe_info_size )
					RETURN_ERR( in.skip( size - nsfe_info_size ) );
				phase = 1;
				info.speed_flags = finfo.speed_flags;
				info.chip_flags  = finfo.chip_flags;
				info.track_count = finfo.track_count;
				this->actual_track_count_ = finfo.track_count;
				info.first_track = finfo.first_track;
				memcpy( info.load_addr, finfo.load_addr, 2 * 3 );
				break;
			}
			
			case BLARGG_4CHAR('K','N','A','B'):
				if ( size > (int) sizeof info.banks )
					return blargg_err_file_corrupt;
				RETURN_ERR( in.read( info.banks, size ) );
				break;
			
			case BLARGG_4CHAR('h','t','u','a'): {
				blargg_vector<char> chars;
				blargg_vector<const char*> strs;
				RETURN_ERR( read_strs( in, size, chars, strs ) );
				int n = strs.size();
				
				if ( n > 3 )
					copy_str( strs [3], info.dumper, sizeof info.dumper );
				
				if ( n > 2 )
					copy_str( strs [2], info.copyright, sizeof info.copyright );
				
				if ( n > 1 )
					copy_str( strs [1], info.author, sizeof info.author );
				
				if ( n > 0 )
					copy_str( strs [0], info.game, sizeof info.game );
				
				break;
			}
			
			case BLARGG_4CHAR('e','m','i','t'):
				RETURN_ERR( track_times.resize( size / 4 ) );
				RETURN_ERR( in.read( track_times.begin(), track_times.size() * 4 ) );
				break;
			
			case BLARGG_4CHAR('l','b','l','t'):
				RETURN_ERR( read_strs( in, size, track_name_data, track_names ) );
				break;
			
			case BLARGG_4CHAR('t','s','l','p'):
				RETURN_ERR( playlist.resize( size ) );
				RETURN_ERR( in.read( &playlist [0], size ) );
				break;
			
			case BLARGG_4CHAR('A','T','A','D'): {
				check( phase == 1 );
				phase = 2;
				if ( !nsf_emu )
				{
					RETURN_ERR( data.resize( size ) );
					RETURN_ERR( in.read( data.begin(), size ) );
				}
				else
				{
					Subset_Reader sub( &in, size ); // limit emu to nsf data
					Remaining_Reader rem( &header, header.size, &sub );
					RETURN_ERR( nsf_emu->Nsf_Emu::load_( rem ) );
					check( rem.remain() == 0 );
				}
				break;
			}
			
			case BLARGG_4CHAR('D','N','E','N'):
				check( phase == 2 );
				phase = 3;
				break;
			
			default:
				// tags that can be skipped start with a lowercase character
				check( islower( (tag >> 24) & 0xFF ) );
				RETURN_ERR( in.skip( size ) );
				break;
		}
	}
	
	return blargg_ok;
}

blargg_err_t Nsfe_Info::track_info_( track_info_t* out, int track ) const
{
	int remapped = remap_track( track );
	if ( (unsigned) remapped < track_times.size() )
	{
		int length = (BOOST::int32_t) get_le32( track_times [remapped] );
		if ( length > 0 )
			out->length = length;
	}
	if ( (unsigned) remapped < track_names.size() )
		Gme_File::copy_field_( out->song, track_names [remapped] );
	
	GME_COPY_FIELD( info, out, game );
	GME_COPY_FIELD( info, out, author );
	GME_COPY_FIELD( info, out, copyright );
	GME_COPY_FIELD( info, out, dumper );
	return blargg_ok;
}

Nsfe_Emu::Nsfe_Emu()
{
	set_type( gme_nsfe_type );
}

Nsfe_Emu::~Nsfe_Emu() { }

void Nsfe_Emu::unload()
{
	info.unload();
	Nsf_Emu::unload();
}

blargg_err_t Nsfe_Emu::track_info_( track_info_t* out, int track ) const
{
	return info.track_info_( out, track );
}

struct Nsfe_File : Gme_Info_
{
	Nsfe_Info info;
	
	Nsfe_File() { set_type( gme_nsfe_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		RETURN_ERR( info.load( in, 0 ) );
		info.disable_playlist( false );
		set_track_count( info.info.track_count );
		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int track ) const
	{
		return info.track_info_( out, track );
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_nsf_file( info.info, info.data.begin(), info.data.end() - info.data.begin(), out );
		return blargg_ok;
	}
};

static Music_Emu* new_nsfe_emu () { return BLARGG_NEW Nsfe_Emu ; }
static Music_Emu* new_nsfe_file() { return BLARGG_NEW Nsfe_File; }

gme_type_t_ const gme_nsfe_type [1] = {{ "Nintendo NES", 0, &new_nsfe_emu, &new_nsfe_file, "NSFE", 1 }};

blargg_err_t Nsfe_Emu::load_( Data_Reader& in )
{
	RETURN_ERR( info.load( in, this ) );
	disable_playlist_( false );
	return blargg_ok;
}

void Nsfe_Emu::disable_playlist_( bool b )
{
	info.disable_playlist( b );
	set_track_count( info.info.track_count );
}

void Nsfe_Emu::clear_playlist_()
{
	disable_playlist_( true );
	Nsf_Emu::clear_playlist_();
}

blargg_err_t Nsfe_Emu::start_track_( int track )
{
	return Nsf_Emu::start_track_( info.remap_track( track ) );
}
