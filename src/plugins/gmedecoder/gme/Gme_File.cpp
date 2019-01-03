// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gme_File.h"

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

void Gme_File::unload()
{
	clear_playlist(); // BEFORE clearing track count
	track_count_     = 0;
	raw_track_count_ = 0;
	Gme_Loader::unload();
}

Gme_File::Gme_File()
{
	type_           = NULL;
	user_data_      = NULL;
	user_cleanup_   = NULL;
	Gme_File::unload(); // clears fields
}

Gme_File::~Gme_File()
{
	if ( user_cleanup_ )
		user_cleanup_( user_data_ );
}

blargg_err_t Gme_File::post_load()
{
	if ( !track_count() )
		set_track_count( type()->track_count );
	return Gme_Loader::post_load();
}

void Gme_File::clear_playlist()
{
	playlist.clear();
	clear_playlist_();
	track_count_ = raw_track_count_;
}

void Gme_File::copy_field_( char out [], const char* in, int in_size )
{
	if ( !in || !*in )
		return;
	
	// remove spaces/junk from beginning
	while ( in_size && unsigned (*in - 1) <= ' ' - 1 )
	{
		in++;
		in_size--;
	}
	
	// truncate
	if ( in_size > max_field_ )
		in_size = max_field_;
	
	// find terminator
	int len = 0;
	while ( len < in_size && in [len] )
		len++;
	
	// remove spaces/junk from end
	while ( len && unsigned (in [len - 1]) <= ' ' )
		len--;
	
	// copy
	out [len] = 0;
	memcpy( out, in, len );
	
	// strip out stupid fields that should have been left blank
	if ( !strcmp( out, "?" ) || !strcmp( out, "<?>" ) || !strcmp( out, "< ? >" ) )
		out [0] = 0;
}

void Gme_File::copy_field_( char out [], const char* in )
{
	copy_field_( out, in, max_field_ );
}

blargg_err_t Gme_File::remap_track_( int* track_io ) const
{
	if ( (unsigned) *track_io >= (unsigned) track_count() )
		return BLARGG_ERR( BLARGG_ERR_CALLER, "invalid track" );
	
	if ( (unsigned) *track_io < (unsigned) playlist.size() )
	{
		M3u_Playlist::entry_t const& e = playlist [*track_io];
		*track_io = 0;
		if ( e.track >= 0 )
		{
			*track_io = e.track;
			// TODO: really needs to be removed?
			//if ( !(type_->flags_ & 0x02) )
			//  *track_io -= e.decimal_track;
		}
		if ( *track_io >= raw_track_count_ )
			return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "invalid track in m3u playlist" );
	}
	else
	{
		check( !playlist.size() );
	}
	return blargg_ok;
}

blargg_err_t Gme_File::track_info( track_info_t* out, int track ) const
{
	out->track_count   = track_count();
	out->length        = -1;
	out->loop_length   = -1;
	out->intro_length  = -1;
	out->fade_length   = -1;
	out->play_length   = -1;
	out->repeat_count  = -1;
	out->song      [0] = 0;
	out->game      [0] = 0;
	out->author    [0] = 0;
	out->composer  [0] = 0;
	out->engineer  [0] = 0;
	out->sequencer [0] = 0;
	out->tagger    [0] = 0;
	out->copyright [0] = 0;
	out->date      [0] = 0;
	out->comment   [0] = 0;
	out->dumper    [0] = 0;
	out->system    [0] = 0;
	out->disc      [0] = 0;
	out->track     [0] = 0;
	out->ost       [0] = 0;
	
	copy_field_( out->system, type()->system );
	
	int remapped = track;
	RETURN_ERR( remap_track_( &remapped ) );
	RETURN_ERR( track_info_( out, remapped ) );
	
	// override with m3u info
	if ( playlist.size() )
	{
		M3u_Playlist::info_t const& i = playlist.info();
		copy_field_( out->game     , i.title );
		copy_field_( out->author   , i.artist );
		copy_field_( out->engineer , i.engineer );
		copy_field_( out->composer , i.composer );
		copy_field_( out->sequencer, i.sequencer );
		copy_field_( out->copyright, i.copyright );
		copy_field_( out->dumper   , i.ripping );
		copy_field_( out->tagger   , i.tagging );
		copy_field_( out->date     , i.date );
		
		M3u_Playlist::entry_t const& e = playlist [track];
		if ( e.length >= 0 ) out->length       = e.length;
		if ( e.intro  >= 0 ) out->intro_length = e.intro;
		if ( e.loop   >= 0 ) out->loop_length  = e.loop;
		if ( e.fade   >= 0 ) out->fade_length  = e.fade;
		if ( e.repeat >= 0 ) out->repeat_count = e.repeat;
		copy_field_( out->song, e.name );
	}
	
	// play_length
	out->play_length = out->length;
	if ( out->play_length <= 0 )
	{
		out->play_length = out->intro_length + 2 * out->loop_length; // intro + 2 loops
		if ( out->play_length <= 0 )
			out->play_length = 150 * 1000; // 2.5 minutes
	}
	
	return blargg_ok;
}
