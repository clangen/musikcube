// File_Extractor 1.0.0. http://www.slack.net/~ant/

#include "Data_Reader.h"

#include "blargg_endian.h"
#include <stdio.h>
#include <errno.h>

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

extern char* blargg_to_utf8( const blargg_wchar_t* wpath );
extern size_t utf16_encode_char( unsigned cur_wchar, blargg_wchar_t * out );
extern size_t utf16_decode_char( const blargg_wchar_t * p_source, unsigned * p_out, size_t p_source_length );
extern size_t utf8_decode_char( const char *p_utf8, unsigned & wide, size_t mmax );
extern size_t utf8_encode_char( unsigned wide, char * target );
extern blargg_wchar_t* blargg_to_wide( const char* path );
extern size_t utf8_char_len_from_header( char p_c );

// Data_Reader

blargg_err_t Data_Reader::read( void* p, long n )
{
	assert( n >= 0 );

	if ( n < 0 )
		return blargg_err_caller;

	if ( n <= 0 )
		return blargg_ok;

	if ( n > remain() )
		return blargg_err_file_eof;

	blargg_err_t err = read_v( p, n );
	if ( !err )
		remain_ -= n;

	return err;
}

blargg_err_t Data_Reader::read_avail( void* p, int* n_ )
{
	assert( *n_ >= 0 );

	long n = (long) min( (BOOST::uint64_t)(*n_), remain() );
	*n_ = 0;

	if ( n < 0 )
		return blargg_err_caller;

	if ( n <= 0 )
		return blargg_ok;

	blargg_err_t err = read_v( p, n );
	if ( !err )
	{
		remain_ -= n;
		*n_ = (int) n;
	}

	return err;
}

blargg_err_t Data_Reader::read_avail( void* p, long* n )
{
	int i = STATIC_CAST(int, *n);
	blargg_err_t err = read_avail( p, &i );
	*n = i;
	return err;
}

blargg_err_t Data_Reader::skip_v( BOOST::uint64_t count )
{
	char buf [512];
	while ( count )
	{
        BOOST::uint64_t n = min( count, (BOOST::uint64_t) sizeof buf );
		count -= n;
		RETURN_ERR( read_v( buf, (long)n ) );
	}
	return blargg_ok;
}

blargg_err_t Data_Reader::skip( long n )
{
	assert( n >= 0 );

	if ( n < 0 )
		return blargg_err_caller;

	if ( n <= 0 )
		return blargg_ok;

	if ( n > remain() )
		return blargg_err_file_eof;

	blargg_err_t err = skip_v( n );
	if ( !err )
		remain_ -= n;

	return err;
}


// File_Reader

blargg_err_t File_Reader::seek( BOOST::uint64_t n )
{
	assert( n >= 0 );

	if ( n == tell() )
		return blargg_ok;

	if ( n > size() )
		return blargg_err_file_eof;

	blargg_err_t err = seek_v( n );
	if ( !err )
		set_tell( n );

	return err;
}

blargg_err_t File_Reader::skip_v( BOOST::uint64_t n )
{
	return seek_v( tell() + n );
}


// Subset_Reader

Subset_Reader::Subset_Reader( Data_Reader* dr, BOOST::uint64_t size ) :
	in( dr )
{
	set_remain( min( size, dr->remain() ) );
}

blargg_err_t Subset_Reader::read_v( void* p, long s )
{
	return in->read( p, s );
}


// Remaining_Reader

Remaining_Reader::Remaining_Reader( void const* h, int size, Data_Reader* r ) :
	in( r )
{
	header        = h;
	header_remain = size;

	set_remain( size + r->remain() );
}

blargg_err_t Remaining_Reader::read_v( void* out, long count )
{
	long first = min( count, header_remain );
	if ( first )
	{
		memcpy( out, header, first );
		header = STATIC_CAST(char const*, header) + first;
		header_remain -= first;
	}

	return in->read( STATIC_CAST(char*, out) + first, count - first );
}


// Mem_File_Reader

Mem_File_Reader::Mem_File_Reader( const void* p, long s ) :
	begin( STATIC_CAST(const char*, p) )
{
	set_size( s );
}

blargg_err_t Mem_File_Reader::read_v( void* p, long s )
{
	memcpy( p, begin + tell(), s );
	return blargg_ok;
}

blargg_err_t Mem_File_Reader::seek_v( BOOST::uint64_t )
{
	return blargg_ok;
}


// Callback_Reader

Callback_Reader::Callback_Reader( callback_t c, BOOST::uint64_t s, void* d ) :
	callback( c ),
	user_data( d )
{
	set_remain( s );
}

blargg_err_t Callback_Reader::read_v( void* out, long count )
{
	return callback( user_data, out, count );
}


// Callback_File_Reader

Callback_File_Reader::Callback_File_Reader( callback_t c, BOOST::uint64_t s, void* d ) :
	callback( c ),
	user_data( d )
{
	set_size( s );
}

blargg_err_t Callback_File_Reader::read_v( void* out, long count )
{
	return callback( user_data, out, count, tell() );
}

blargg_err_t Callback_File_Reader::seek_v( BOOST::uint64_t )
{
	return blargg_ok;
}

static const BOOST::uint8_t mask_tab[6]={0x80,0xE0,0xF0,0xF8,0xFC,0xFE};

static const BOOST::uint8_t val_tab[6]={0,0xC0,0xE0,0xF0,0xF8,0xFC};

#ifdef _WIN32

static FILE* blargg_fopen( const char path [], const char mode [] )
{
	FILE* file = NULL;
    blargg_wchar_t* wmode = NULL;
    blargg_wchar_t* wpath = NULL;

	wpath = blargg_to_wide( path );
	if ( wpath )
	{
		wmode = blargg_to_wide( mode );
		if (wmode)
#if _MSC_VER >= 1300
			errno = _wfopen_s(&file, wpath, wmode);
#else
			file = _wfopen( wpath, wmode );
#endif
	}

	// Save and restore errno in case free() clears it
	int saved_errno = errno;
	free( wmode );
	free( wpath );
	errno = saved_errno;

	return file;
}

#else

static inline FILE* blargg_fopen( const char path [], const char mode [] )
{
	return fopen( path, mode );
}

#endif


// Std_File_Reader

Std_File_Reader::Std_File_Reader()
{
	file_ = NULL;
}

Std_File_Reader::~Std_File_Reader()
{
	close();
}

static blargg_err_t blargg_fopen( FILE** out, const char path [] )
{
	errno = 0;
	*out = blargg_fopen( path, "rb" );
	if ( !*out )
	{
		#ifdef ENOENT
			if ( errno == ENOENT )
				return blargg_err_file_missing;
		#endif
		#ifdef ENOMEM
			if ( errno == ENOMEM )
				return blargg_err_memory;
		#endif
		return blargg_err_file_read;
	}

	return blargg_ok;
}

static blargg_err_t blargg_fsize( FILE* f, long* out )
{
	if ( fseek( f, 0, SEEK_END ) )
		return blargg_err_file_io;

	*out = ftell( f );
	if ( *out < 0 )
		return blargg_err_file_io;

	if ( fseek( f, 0, SEEK_SET ) )
		return blargg_err_file_io;

	return blargg_ok;
}

blargg_err_t Std_File_Reader::open( const char path [] )
{
	close();

	FILE* f;
	RETURN_ERR( blargg_fopen( &f, path ) );

	long s;
	blargg_err_t err = blargg_fsize( f, &s );
	if ( err )
	{
		fclose( f );
		return err;
	}

	file_ = f;
	set_size( s );

	return blargg_ok;
}

void Std_File_Reader::make_unbuffered()
{
#ifdef _WIN32
    BOOST::uint64_t offset = _ftelli64( STATIC_CAST(FILE*, file_) );
#else
    BOOST::uint64_t offset = ftello( STATIC_CAST(FILE*, file_) );
#endif
	if ( setvbuf( STATIC_CAST(FILE*, file_), NULL, _IONBF, 0 ) )
		check( false ); // shouldn't fail, but OK if it does
#ifdef _WIN32
    _fseeki64( STATIC_CAST(FILE*, file_), offset, SEEK_SET );
#else
    fseeko( STATIC_CAST(FILE*, file_), offset, SEEK_SET );
#endif
}

blargg_err_t Std_File_Reader::read_v( void* p, long s )
{
	if ( (size_t) s != fread( p, 1, s, STATIC_CAST(FILE*, file_) ) )
	{
		// Data_Reader's wrapper should prevent EOF
		check( !feof( STATIC_CAST(FILE*, file_) ) );

		return blargg_err_file_io;
	}

	return blargg_ok;
}

blargg_err_t Std_File_Reader::seek_v( BOOST::uint64_t n )
{
#ifdef _WIN32
	if ( _fseeki64( STATIC_CAST(FILE*, file_), n, SEEK_SET ) )
#else
    if ( fseeko( STATIC_CAST(FILE*, file_), n, SEEK_SET ) )
#endif
	{
		// Data_Reader's wrapper should prevent EOF
		check( !feof( STATIC_CAST(FILE*, file_) ) );

		return blargg_err_file_io;
	}

	return blargg_ok;
}

void Std_File_Reader::close()
{
	if ( file_ )
	{
		fclose( STATIC_CAST(FILE*, file_) );
		file_ = NULL;
	}
}


// Gzip_File_Reader

#ifdef HAVE_ZLIB_H

#include "zlib.h"

static const char* get_gzip_eof( const char path [], long* eof )
{
	FILE* file;
	RETURN_ERR( blargg_fopen( &file, path ) );

	int const h_size = 4;
	unsigned char h [h_size];

	// read four bytes to ensure that we can seek to -4 later
	if ( fread( h, 1, h_size, file ) != (size_t) h_size || h[0] != 0x1F || h[1] != 0x8B )
	{
		// Not gzipped
		if ( ferror( file ) )
			return blargg_err_file_io;

		if ( fseek( file, 0, SEEK_END ) )
			return blargg_err_file_io;

		*eof = ftell( file );
		if ( *eof < 0 )
			return blargg_err_file_io;
	}
	else
	{
		// Gzipped; get uncompressed size from end
		if ( fseek( file, -h_size, SEEK_END ) )
			return blargg_err_file_io;

		if ( fread( h, 1, h_size, file ) != (size_t) h_size )
			return blargg_err_file_io;

		*eof = get_le32( h );
	}

	if ( fclose( file ) )
		check( false );

	return blargg_ok;
}

Gzip_File_Reader::Gzip_File_Reader()
{
	file_ = NULL;
}

Gzip_File_Reader::~Gzip_File_Reader()
{
	close();
}

blargg_err_t Gzip_File_Reader::open( const char path [] )
{
	close();

	long s;
	RETURN_ERR( get_gzip_eof( path, &s ) );

	file_ = gzopen( path, "rb" );
	if ( !file_ )
		return blargg_err_file_read;

	set_size( s );
	return blargg_ok;
}

static blargg_err_t convert_gz_error( gzFile file )
{
	int err;
	gzerror( file, &err );

	switch ( err )
	{
	case Z_STREAM_ERROR:    break;
	case Z_DATA_ERROR:      return blargg_err_file_corrupt;
	case Z_MEM_ERROR:       return blargg_err_memory;
	case Z_BUF_ERROR:       break;
	}
	return blargg_err_internal;
}

blargg_err_t Gzip_File_Reader::read_v( void* p, long s )
{
    while ( s > 0 )
    {
        int s_i = (int)( s > INT_MAX ? INT_MAX : s );
        int result = gzread( (gzFile) file_, p, s_i );
        if ( result != s_i )
        {
            if ( result < 0 )
                return convert_gz_error( (gzFile) file_ );

            return blargg_err_file_corrupt;
        }
        p = (char*)p + result;
        s -= result;
    }

	return blargg_ok;
}

blargg_err_t Gzip_File_Reader::seek_v( BOOST::uint64_t n )
{
    if ( gzseek( (gzFile) file_, (long)n, SEEK_SET ) < 0 )
        return convert_gz_error( (gzFile) file_ );

	return blargg_ok;
}

void Gzip_File_Reader::close()
{
	if ( file_ )
	{
        if ( gzclose( (gzFile) file_ ) )
			check( false );
		file_ = NULL;
	}
}

#endif
