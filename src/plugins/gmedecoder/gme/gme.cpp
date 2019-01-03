// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Music_Emu.h"

#if !GME_DISABLE_EFFECTS
#include "Effects_Buffer.h"
#endif
#include "blargg_endian.h"
#include <string.h>
#include <ctype.h>

/* Copyright (C) 2003-2009 Shay Green. This module is free software; you
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

#ifndef GME_TYPE_LIST

// Default list of all supported game music types (copy this to blargg_config.h
// if you want to modify it)
#define GME_TYPE_LIST \
	gme_ay_type,\
	gme_gbs_type,\
	gme_gym_type,\
	gme_hes_type,\
	gme_kss_type,\
	gme_nsf_type,\
	gme_nsfe_type,\
	gme_sap_type,\
    gme_sfm_type,\
	gme_sgc_type,\
	gme_spc_type,\
	gme_vgm_type,\
	gme_vgz_type

#endif

static gme_type_t const gme_type_list_ [] = { GME_TYPE_LIST, 0 };

gme_type_t const* gme_type_list()
{
	return gme_type_list_;
}

const char* gme_identify_header( void const* header )
{
	switch ( get_be32( header ) )
	{
		case BLARGG_4CHAR('Z','X','A','Y'):  return "AY";
		case BLARGG_4CHAR('G','B','S',0x01):
		case BLARGG_4CHAR('G','B','S',0x02): return "GBS";
		case BLARGG_4CHAR('G','Y','M','X'):  return "GYM";
		case BLARGG_4CHAR('H','E','S','M'):  return "HES";
		case BLARGG_4CHAR('K','S','C','C'):
		case BLARGG_4CHAR('K','S','S','X'):  return "KSS";
		case BLARGG_4CHAR('N','E','S','M'):  return "NSF";
		case BLARGG_4CHAR('N','S','F','E'):  return "NSFE";
		case BLARGG_4CHAR('S','A','P',0x0D): return "SAP";
        case BLARGG_4CHAR('S','F','M','1'):  return "SFM";
		case BLARGG_4CHAR('S','G','C',0x1A): return "SGC";
		case BLARGG_4CHAR('S','N','E','S'):  return "SPC";
		case BLARGG_4CHAR('V','g','m',' '):  return "VGM";
	}
	return "";
}

static void to_uppercase( const char in [], int len, char out [] )
{
	for ( int i = 0; i < len; i++ )
	{
		if ( !(out [i] = toupper( in [i] )) )
			return;
	}
	*out = 0; // extension too long
}

gme_type_t gme_identify_extension( const char extension_ [] )
{
	char const* end = strrchr( extension_, '.' );
	if ( end )
		extension_ = end + 1;
	
	char extension [6];
	to_uppercase( extension_, sizeof extension, extension );
	
	gme_type_t const* types = gme_type_list_;
	for ( ; *types; types++ )
		if ( !strcmp( extension, (*types)->extension_ ) )
			break;
	return *types;
}

gme_err_t gme_identify_file( const char path [], gme_type_t* type_out )
{
	*type_out = gme_identify_extension( path );
	// TODO: don't examine header if file has extension?
	if ( !*type_out )
	{
		char header [4];
		GME_FILE_READER in;
		RETURN_ERR( in.open( path ) );
		RETURN_ERR( in.read( header, sizeof header ) );
		*type_out = gme_identify_extension( gme_identify_header( header ) );
	}
	return blargg_ok;   
}

gme_err_t gme_open_data( void const* data, long size, Music_Emu** out, int sample_rate )
{
	require( (data || !size) && out );
	*out = NULL;
	
	gme_type_t file_type = 0;
	if ( size >= 4 )
		file_type = gme_identify_extension( gme_identify_header( data ) );
	if ( !file_type )
		return blargg_err_file_type;
	
	Music_Emu* emu = gme_new_emu( file_type, sample_rate );
	CHECK_ALLOC( emu );
	
	gme_err_t err = gme_load_data( emu, data, size );
	
	if ( err )
		delete emu;
	else
		*out = emu;
	
	return err;
}

gme_err_t gme_open_file( const char path [], Music_Emu** out, int sample_rate )
{
	require( path && out );
	*out = NULL;
	
	GME_FILE_READER in;
	RETURN_ERR( in.open( path ) );
	
	char header [4];
	int header_size = 0;
	
	gme_type_t file_type = gme_identify_extension( path );
	if ( !file_type )
	{
		header_size = sizeof header;
		RETURN_ERR( in.read( header, sizeof header ) );
		file_type = gme_identify_extension( gme_identify_header( header ) );
	}
	if ( !file_type )
		return blargg_err_file_type;
	
	Music_Emu* emu = gme_new_emu( file_type, sample_rate );
	CHECK_ALLOC( emu );
	
	// optimization: avoids seeking/re-reading header
	Remaining_Reader rem( header, header_size, &in );
	gme_err_t err = emu->load( rem );
	in.close();
	
	if ( err )
		delete emu;
	else
		*out = emu;
	
	return err;
}

Music_Emu* gme_new_emu( gme_type_t type, int rate )
{
	if ( type )
	{
		if ( rate == gme_info_only )
			return type->new_info();
		
		Music_Emu* gme = type->new_emu();
		if ( gme )
		{
		#if !GME_DISABLE_EFFECTS
			if ( type->flags_ & 1 )
			{
				gme->effects_buffer_ = BLARGG_NEW Simple_Effects_Buffer;
				if ( gme->effects_buffer_ )
					gme->set_buffer( gme->effects_buffer_ );
			}
			
			if ( !(type->flags_ & 1) || gme->effects_buffer_ )
		#endif
			{
				if ( !gme->set_sample_rate( rate ) )
				{
					check( gme->type() == type );
					return gme;
				}
			}
			delete gme;
		}
	}
	return NULL;
}

gme_err_t gme_load_file( Music_Emu* gme, const char path [] ) { return gme->load_file( path ); }

gme_err_t gme_load_data( Music_Emu* gme, void const* data, long size )
{
	Mem_File_Reader in( data, size );
	return gme->load( in );
}

gme_err_t gme_load_custom( Music_Emu* gme, gme_reader_t func, long size, void* data )
{
	Callback_Reader in( func, size, data );
	return gme->load( in );
}

void gme_delete( Music_Emu* gme ) { delete gme; }

gme_type_t gme_type( Music_Emu const* gme ) { return gme->type(); }

const char* gme_type_system( gme_type_t_ const* type ) { return type->system; }

const char* gme_warning( Music_Emu* gme ) { return gme->warning(); }

int gme_track_count( Music_Emu const* gme ) { return gme->track_count(); }

struct gme_info_t_ : gme_info_t
{
	track_info_t info;
	
	BLARGG_DISABLE_NOTHROW
};

gme_err_t gme_track_info( Music_Emu const* me, gme_info_t** out, int track )
{
	*out = NULL;
	
	gme_info_t_* info = BLARGG_NEW gme_info_t_;
	CHECK_ALLOC( info );
	
	gme_err_t err = me->track_info( &info->info, track );
	if ( err )
	{
		gme_free_info( info );
		return err;
	}
	
	#define COPY(name) info->name = info->info.name;
	
	COPY( length );
	COPY( intro_length );
	COPY( loop_length );
	
	info->i4  = -1;
	info->i5  = -1;
	info->i6  = -1;
	info->i7  = -1;
	info->i8  = -1;
	info->i9  = -1;
	info->i10 = -1;
	info->i11 = -1;
	info->i12 = -1;
	info->i13 = -1;
	info->i14 = -1;
	info->i15 = -1;
	
	info->s7  = "";
	info->s8  = "";
	info->s9  = "";
	info->s10 = "";
	info->s11 = "";
	info->s12 = "";
	info->s13 = "";
	info->s14 = "";
	info->s15 = "";
	
	COPY( system );
	COPY( game );
	COPY( song );
	COPY( author );
	COPY( copyright );
	COPY( comment );
	COPY( dumper );
	
	#undef COPY
	
	info->play_length = info->length;
	if ( info->play_length <= 0 )
	{
		info->play_length = info->intro_length + 2 * info->loop_length; // intro + 2 loops
		if ( info->play_length <= 0 )
			info->play_length = 150 * 1000; // 2.5 minutes
	}
	
	*out = info;
	
	return blargg_ok;
}

gme_err_t gme_set_track_info( Music_Emu * me, gme_info_t* in, int track )
{
	track_info_t* info = BLARGG_NEW track_info_t;
	CHECK_ALLOC( info );
	
#define COPY(name) info->name = in->name;
	
	COPY( length );
	COPY( intro_length );
	COPY( loop_length );
	
#undef COPY
#define COPY(name) if ( in->name ) strncpy( info->name, in->name, sizeof(info->name) - 1 ), info->name[sizeof(info->name)-1] = '\0'; else info->name[0] = '\0';
    
	COPY( system );
	COPY( game );
	COPY( song );
	COPY( author );
	COPY( copyright );
	COPY( comment );
	COPY( dumper );
	
#undef COPY
	
	blargg_err_t err = me->set_track_info( info, track );
    
    delete info;
    
    return err;
}

void gme_free_info( gme_info_t* info )
{
	delete STATIC_CAST(gme_info_t_*,info);
}

void*     gme_user_data      ( Music_Emu const* gme )                   { return gme->user_data(); }
void      gme_set_user_data  ( Music_Emu* gme, void* new_user_data )    { gme->set_user_data( new_user_data ); }
void      gme_set_user_cleanup(Music_Emu* gme, gme_user_cleanup_t func ){ gme->set_user_cleanup( func ); }

gme_err_t gme_start_track    ( Music_Emu* gme, int index )              { return gme->start_track( index ); }
gme_err_t gme_play           ( Music_Emu* gme, int n, short p [] )      { return gme->play( n, p ); }
void      gme_set_fade       ( Music_Emu* gme, int start_msec, int length_msec ) { gme->set_fade( start_msec, length_msec ); }
gme_bool  gme_track_ended    ( Music_Emu const* gme )                   { return gme->track_ended(); }
int       gme_tell           ( Music_Emu const* gme )                   { return gme->tell(); }
gme_err_t gme_seek           ( Music_Emu* gme, int msec )               { return gme->seek( msec ); }
gme_err_t gme_skip           ( Music_Emu* gme, int samples )            { return gme->skip( samples ); }
int       gme_voice_count    ( Music_Emu const* gme )                   { return gme->voice_count(); }
void      gme_ignore_silence ( Music_Emu* gme, gme_bool disable )       { gme->ignore_silence( disable != 0 ); }
void      gme_set_tempo      ( Music_Emu* gme, double t )               { gme->set_tempo( t ); }
void      gme_mute_voice     ( Music_Emu* gme, int index, gme_bool mute ){ gme->mute_voice( index, mute != 0 ); }
void      gme_mute_voices    ( Music_Emu* gme, int mask )               { gme->mute_voices( mask ); }
void      gme_set_equalizer  ( Music_Emu* gme, gme_equalizer_t const* eq ) { gme->set_equalizer( *eq ); }
void      gme_equalizer      ( Music_Emu const* gme, gme_equalizer_t* o )  { *o = gme->equalizer(); }
const char* gme_voice_name   ( Music_Emu const* gme, int i )            { return gme->voice_name( i ); }
gme_err_t gme_save           ( Music_Emu const* gme, gme_writer_t writer, void* your_data ) { return gme->save( writer, your_data ); }

void gme_effects( Music_Emu const* gme, gme_effects_t* out )
{
	static gme_effects_t const zero = { 0, 0, 0,0,0,0,0,0, 0, 0, 0,0,0,0,0,0 };
	*out = zero;

	#if !GME_DISABLE_EFFECTS
	{
		Simple_Effects_Buffer* b = STATIC_CAST(Simple_Effects_Buffer*,gme->effects_buffer_);
		if ( b )
		{
			out->enabled  = b->config().enabled;
			out->echo     = b->config().echo;
			out->stereo   = b->config().stereo;
			out->surround = b->config().surround;
		}
	}
	#endif
}

void gme_set_effects( Music_Emu* gme, gme_effects_t const* in )
{
	#if !GME_DISABLE_EFFECTS
	{
		Simple_Effects_Buffer* b = STATIC_CAST(Simple_Effects_Buffer*,gme->effects_buffer_);
		if ( b )
		{
			b->config().enabled = false;
			if ( in )
			{
				b->config().enabled  = in->enabled;
				b->config().echo     = in->echo;
				b->config().stereo   = in->stereo;
				b->config().surround = in->surround;
			}
			b->apply_config();
		}
	}
	#endif
}

void gme_set_stereo_depth( Music_Emu* gme, double depth )
{
	#if !GME_DISABLE_EFFECTS
	{
		if ( gme->effects_buffer_ )
		{
			gme_effects_t cfg;
			gme_effects( gme, &cfg );
			cfg.enabled  = (depth > 0.0);
			cfg.echo     = depth;
			cfg.stereo   = depth;
			cfg.surround = true;
			gme_set_effects( gme, &cfg );
		}
	}
	#endif
}

#define ENTRY( name ) { blargg_err_##name, gme_err_##name }
static blargg_err_to_code_t const gme_codes [] =
{
	ENTRY( generic ),
	ENTRY( memory ),
	ENTRY( caller ),
	ENTRY( internal ),
	ENTRY( limitation ),
	
	ENTRY( file_missing ),
	ENTRY( file_read ),
	ENTRY( file_io ),
	ENTRY( file_eof ),
	
	ENTRY( file_type ),
	ENTRY( file_feature ),
	ENTRY( file_corrupt ),
	
	{ 0, -1 }
};
#undef ENTRY

static int err_code( gme_err_t err )
{
	return blargg_err_to_code( err, gme_codes );
}

int gme_err_code( gme_err_t err )
{
	int code = err_code( err );
	return (code >= 0 ? code : gme_err_generic);
}

gme_err_t gme_code_to_err( int code )
{
	return blargg_code_to_err( code, gme_codes );
}

const char* gme_err_details( gme_err_t err )
{
	// If we don't have error code assigned, return entire string
	return (err_code( err ) >= 0 ? blargg_err_details( err ) : blargg_err_str( err ));
}

const char* gme_err_str( gme_err_t err )
{
	return blargg_err_str( err );
}
