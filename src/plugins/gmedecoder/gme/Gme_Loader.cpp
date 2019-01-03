// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gme_Loader.h"

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

void Gme_Loader::unload()
{
	file_begin_ = NULL;
	file_end_   = NULL;
	file_data.clear();
}

Gme_Loader::Gme_Loader()
{
	warning_ = NULL;
	Gme_Loader::unload();
	blargg_verify_byte_order(); // used by most emulator types, so save them the trouble
}

Gme_Loader::~Gme_Loader() { }

blargg_err_t Gme_Loader::load_mem_( byte const data [], int size )
{
	require( data != file_data.begin() ); // load_mem_() or load_() must be overridden
	Mem_File_Reader in( data, size );
	return load_( in );
}

inline blargg_err_t Gme_Loader::load_mem_wrapper( byte const data [], int size )
{
	file_begin_ = data;
	file_end_   = data + size;
	return load_mem_( data, size );
}

blargg_err_t Gme_Loader::load_( Data_Reader& in )
{
	RETURN_ERR( file_data.resize( in.remain() ) );
	RETURN_ERR( in.read( file_data.begin(), file_data.size() ) );
	return load_mem_wrapper( file_data.begin(), file_data.size() );
}

blargg_err_t Gme_Loader::post_load_( blargg_err_t err )
{
	if ( err )
	{
		unload();
		return err;
	}
	
	return post_load();
}

blargg_err_t Gme_Loader::load_mem( void const* in, long size )
{
	pre_load();
	return post_load_( load_mem_wrapper( (byte const*) in, (int) size ) );
}

blargg_err_t Gme_Loader::load( Data_Reader& in )
{
	pre_load();
	return post_load_( load_( in ) );
}

blargg_err_t Gme_Loader::load_file( const char path [] )
{
	pre_load();
	GME_FILE_READER in;
	RETURN_ERR( in.open( path ) );
	return post_load_( load_( in ) );
}
