// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "M3u_Playlist.h"
#include "Music_Emu.h"

/* Copyright (C) 2006 Shay Green. This module is free software; you
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

// gme functions defined here to avoid linking in m3u code unless it's used

blargg_err_t Gme_File::load_m3u_( blargg_err_t err )
{
	if ( !err )
	{
		require( raw_track_count_ ); // file must be loaded first
		if ( playlist.size() )
			track_count_ = playlist.size();
		
		int line = playlist.first_error();
		if ( line )
		{
			// avoid using bloated printf()
			char* out = &playlist_warning [sizeof playlist_warning];
			*--out = 0;
			do {
				*--out = line % 10 + '0';
			} while ( (line /= 10) > 0 );
			
			static const char str [] = "Problem in m3u at line ";
			out -= sizeof str - 1;
			memcpy( out, str, sizeof str - 1 );
			set_warning( out );
		}
	}
	return err;
}

blargg_err_t Gme_File::load_m3u( const char path [] ) { return load_m3u_( playlist.load( path ) ); }

blargg_err_t Gme_File::load_m3u( Data_Reader& in )  { return load_m3u_( playlist.load( in ) ); }

gme_err_t gme_load_m3u( Music_Emu* me, const char path [] ) { return me->load_m3u( path ); }

gme_err_t gme_load_m3u_data( Music_Emu* me, const void* data, long size )
{
	Mem_File_Reader in( data, size );
	return me->load_m3u( in );
}

static char* skip_white( char* in )
{
	while ( unsigned (*in - 1) <= ' ' - 1 )
		in++;
	return in;
}

inline unsigned from_dec( unsigned n ) { return n - '0'; }

static char* parse_filename( char* in, M3u_Playlist::entry_t& entry )
{
	entry.file = in;
	entry.type = "";
	char* out = in;
	while ( 1 )
	{
		int c = *in;
		if ( !c ) break;
		in++;
		
		if ( c == ',' ) // commas in filename
		{
			char* p = skip_white( in );
			if ( *p == '$' || from_dec( *p ) <= 9 )
			{
				in = p;
				break;
			}
		}
		
		if ( c == ':' && in [0] == ':' && in [1] && in [2] != ',' ) // ::type suffix
		{
			entry.type = ++in;
			while ( (c = *in) != 0 && c != ',' )
				in++;
			if ( c == ',' )
			{
				*in++ = 0; // terminate type
				in = skip_white( in );
			}
			break;
		}
		
		if ( c == '\\' ) // \ prefix for special characters
		{
			c = *in;
			if ( !c ) break;
			in++;
		}
		*out++ = (char) c;
	}
	*out = 0; // terminate string
	return in;
}

static char* next_field( char* in, int* result )
{
	while ( 1 )
	{
		in = skip_white( in );
		
		if ( !*in )
			break;
		
		if ( *in == ',' )
		{
			in++;
			break;
		}
		
		*result = 1;
		in++;
	}
	return skip_white( in );
}

static char* parse_int_( char* in, int* out )
{
	int n = 0;
	while ( 1 )
	{
		unsigned d = from_dec( *in );
		if ( d > 9 )
			break;
		in++;
		n = n * 10 + d;
		*out = n;
	}
	return in;
}

static char* parse_int( char* in, int* out, int* result )
{
	return next_field( parse_int_( in, out ), result );
}

// Returns 16 or greater if not hex
inline int from_hex_char( int h )
{
	h -= 0x30;
	if ( (unsigned) h > 9 )
		h = ((h - 0x11) & 0xDF) + 10;
	return h;
}

static char* parse_track( char* in, M3u_Playlist::entry_t& entry, int* result )
{
	if ( *in == '$' )
	{
		in++;
		int n = 0;
		while ( 1 )
		{
			int h = from_hex_char( *in );
			if ( h > 15 )
				break;
			in++;
			n = n * 16 + h;
			entry.track = n;
		}
	}
	else
	{
		in = parse_int_( in, &entry.track );
		if ( entry.track >= 0 )
			entry.decimal_track = 1;
	}
	return next_field( in, result );
}

static char* parse_time_( char* in, int* out )
{
	*out = -1;
	int n = -1;
	in = parse_int_( in, &n );
	if ( n >= 0 )
	{
		*out = n;
		while ( *in == ':' )
		{
			n = -1;
			in = parse_int_( in + 1, &n );
			if ( n >= 0 )
				*out = *out * 60 + n;
		}
		*out *= 1000;
		if ( *in == '.' )
		{
			n = -1;
			in = parse_int_( in + 1, &n );
			if ( n >= 0 )
				*out = *out + n; 
		}
	}
	return in;
}

static char* parse_time( char* in, int* out, int* result )
{
	return next_field( parse_time_( in, out ), result );
}

static char* parse_name( char* in )
{
	char* out = in;
	while ( 1 )
	{
		int c = *in;
		if ( !c ) break;
		in++;
		
		if ( c == ',' ) // commas in string
		{
			char* p = skip_white( in );
			if ( *p == ',' || *p == '-' || from_dec( *p ) <= 9 )
			{
				in = p;
				break;
			}
		}
		
		if ( c == '\\' ) // \ prefix for special characters
		{
			c = *in;
			if ( !c ) break;
			in++;
		}
		*out++ = (char) c;
	}
	*out = 0; // terminate string
	return in;
}

static int parse_line( char* in, M3u_Playlist::entry_t& entry )
{
	int result = 0;
	
	// file
	entry.file = in;
	entry.type = "";
	in = parse_filename( in, entry );
	
	// track
	entry.track = -1;
	entry.decimal_track = 0;
	in = parse_track( in, entry, &result );
	
	// name
	entry.name = in;
	in = parse_name( in );
	
	// time
	entry.length = -1;
	in = parse_time( in, &entry.length, &result );
	
	// loop
	entry.intro = -1;
	entry.loop  = -1;
	if ( *in == '-' )
	{
		entry.loop = entry.length;
		in++;
	}
	else
	{
		in = parse_time_( in, &entry.loop );
		if ( entry.loop >= 0 )
		{
			entry.intro = entry.length - entry.loop;
			if ( *in == '-' ) // trailing '-' means that intro length was specified 
			{
				in++;
				entry.intro = entry.loop;
				entry.loop  = entry.length - entry.intro;
			}
		}
	}
	in = next_field( in, &result );
	
	// fade
	entry.fade = -1;
	in = parse_time( in, &entry.fade, &result );
	
	// repeat
	entry.repeat = -1;
	in = parse_int( in, &entry.repeat, &result );
	
	return result;
}

static void parse_comment( char* in, M3u_Playlist::info_t& info, char *& last_comment_value, bool first )
{
	in = skip_white( in + 1 );
	const char* field = in;
	if ( *field != '@' )
		while ( *in && *in != ':' )
			in++;
	
	if ( *in == ':' )
	{
		const char* text = skip_white( in + 1 );
		if ( *text )
		{
			*in = 0;
			     if ( !strcmp( "Composer" , field ) ) info.composer  = text;
			else if ( !strcmp( "Engineer" , field ) ) info.engineer  = text;
			else if ( !strcmp( "Ripping"  , field ) ) info.ripping   = text;
			else if ( !strcmp( "Tagging"  , field ) ) info.tagging   = text;
			else if ( !strcmp( "Game"     , field ) ) info.title     = text;
			else if ( !strcmp( "Artist"   , field ) ) info.artist    = text;
			else if ( !strcmp( "Copyright", field ) ) info.copyright = text;
			else
				text = 0;
			if ( text )
				return;
			*in = ':';
		}
	}
	else if ( *field == '@' )
	{
		++field;
		in = (char*)field;
		while ( *in && *in > ' ' )
			in++;
		const char* text = skip_white( in );
		if ( *text )
		{
			char saved = *in;
			*in = 0;
			     if ( !strcmp( "TITLE" ,    field ) ) info.title     = text;
			else if ( !strcmp( "ARTIST",    field ) ) info.artist    = text;
			else if ( !strcmp( "DATE",      field ) ) info.date      = text;
			else if ( !strcmp( "COMPOSER",  field ) ) info.composer  = text;
			else if ( !strcmp( "SEQUENCER", field ) ) info.sequencer = text;
			else if ( !strcmp( "ENGINEER",  field ) ) info.engineer  = text;
			else if ( !strcmp( "RIPPER",    field ) ) info.ripping   = text;
			else if ( !strcmp( "TAGGER",    field ) ) info.tagging   = text;
			else
				text = 0;
			if ( text )
			{
				last_comment_value = (char*)text;
				return;
			}
			*in = saved;
		}
	}
	else if ( last_comment_value )
	{
		size_t len = strlen( last_comment_value );
		last_comment_value[ len ] = ',';
		last_comment_value[ len + 1 ] = ' ';
		size_t field_len = strlen( field );
		memmove( last_comment_value + len + 2, field, field_len );
		last_comment_value[ len + 2 + field_len ] = 0;
		return;
	}
	
	if ( first )
		info.title = field;
}

blargg_err_t M3u_Playlist::parse_()
{
	info_.title     = "";
	info_.artist    = "";
	info_.date      = "";
	info_.composer  = "";
	info_.sequencer = "";
	info_.engineer  = "";
	info_.ripping   = "";
	info_.tagging   = "";
	info_.copyright = "";
	
	int const CR = 13;
	int const LF = 10;
	
	data.end() [-1] = LF; // terminate input
	
	first_error_ = 0;
	bool first_comment = true;
	int line  = 0;
	int count = 0;
	char* in  = data.begin();
	char* last_comment_value = 0;
	while ( in < data.end() )
	{
		// find end of line and terminate it
		line++;
		char* begin = in;
		while ( *in != CR && *in != LF )
		{
			if ( !*in )
				return blargg_err_file_type;
			in++;
		}
		if ( in [0] == CR && in [1] == LF ) // treat CR,LF as a single line
			*in++ = 0;
		*in++ = 0;
		
		// parse line
		if ( *begin == '#' )
		{
			parse_comment( begin, info_, last_comment_value, first_comment );
			first_comment = false;
		}
		else if ( *begin )
		{
			if ( (int) entries.size() <= count )
				RETURN_ERR( entries.resize( count * 2 + 64 ) );
			
			if ( !parse_line( begin, entries [count] ) )
				count++;
			else if ( !first_error_ )
				first_error_ = line;
			first_comment = false;
		}
		else last_comment_value = 0;
	}
	if ( count <= 0 )
		return blargg_err_file_type;
	
	// Treat first comment as title only if another field is also specified
	if ( !(info_.artist [0] | info_.composer [0] | info_.date [0] | info_.engineer [0] | info_.ripping [0] | info_.sequencer [0] | info_.tagging [0] | info_.copyright[0]) )
		info_.title = "";
	
	return entries.resize( count );
}

blargg_err_t M3u_Playlist::parse()
{
	blargg_err_t err = parse_();
	if ( err )
		clear_();
	return err;
}

blargg_err_t M3u_Playlist::load( Data_Reader& in )
{
	RETURN_ERR( data.resize( in.remain() + 1 ) );
	RETURN_ERR( in.read( data.begin(), data.size() - 1 ) );
	return parse();
}

blargg_err_t M3u_Playlist::load( const char path [] )
{
	GME_FILE_READER in;
	RETURN_ERR( in.open( path ) );
	return load( in );
}

blargg_err_t M3u_Playlist::load( void const* in, long size )
{
	RETURN_ERR( data.resize( size + 1 ) );
	memcpy( data.begin(), in, size );
	return parse();
}
