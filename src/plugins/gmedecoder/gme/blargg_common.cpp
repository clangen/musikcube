// $package. http://www.slack.net/~ant/

#include "blargg_common.h"

/* Copyright (C) 2008-2009 Shay Green. This module is free software; you
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

BLARGG_NAMESPACE_BEGIN

// defined here to avoid need for blargg_errors.cpp in simple programs
blargg_err_def_t blargg_err_memory = BLARGG_ERR_MEMORY;

void blargg_vector_::init()
{
	begin_ = NULL;
	size_  = 0;
}

void blargg_vector_::clear()
{
	void* p = begin_;
	begin_  = NULL;
	size_   = 0;
	free( p );
}

blargg_err_t blargg_vector_::resize_( size_t n, size_t elem_size )
{
	if ( n != size_ )
	{
		if ( n == 0 )
		{
			// Simpler to handle explicitly. Realloc will handle a size of 0,
			// but then we have to avoid raising an error for a NULL return.
			clear();
		}
		else
		{
			void* p = realloc( begin_, n * elem_size );
			CHECK_ALLOC( p );
			begin_ = p;
			size_  = n;
		}
	}
	return blargg_ok;
}

static const BOOST::uint8_t mask_tab[6]={0x80,0xE0,0xF0,0xF8,0xFC,0xFE};

static const BOOST::uint8_t val_tab[6]={0,0xC0,0xE0,0xF0,0xF8,0xFC};

size_t utf8_char_len_from_header( char p_c )
{
	size_t cnt = 0;
	for(;;)
	{
		if ( ( p_c & mask_tab[cnt] ) == val_tab[cnt] ) break;
		if ( ++cnt >= 6 ) return 0;
	}

	return cnt + 1;
}

size_t utf8_decode_char( const char *p_utf8, unsigned & wide, size_t mmax )
{
	const BOOST::uint8_t * utf8 = ( const BOOST::uint8_t* )p_utf8;

	if ( mmax == 0 )
	{
		wide = 0;
		return 0;
	}

	if ( utf8[0] < 0x80 )
	{
		wide = utf8[0];
		return utf8[0]>0 ? 1 : 0;
	}
	if ( mmax > 6 ) mmax = 6;
	wide = 0;

	unsigned res=0;
	unsigned n;
	unsigned cnt=0;
	for(;;)
	{
		if ( ( *utf8 & mask_tab[cnt] ) == val_tab[cnt] ) break;
		if ( ++cnt >= mmax ) return 0;
	}
	cnt++;

	if ( cnt==2 && !( *utf8 & 0x1E ) ) return 0;

	if ( cnt == 1 )
		res = *utf8;
	else
		res = ( 0xFF >> ( cnt + 1 ) ) & *utf8;

	for ( n = 1; n < cnt; n++ )
	{
		if ( ( utf8[n] & 0xC0 ) != 0x80 )
			return 0;
		if ( !res && n == 2 && !( ( utf8[n] & 0x7F ) >> ( 7 - cnt ) ) )
			return 0;

		res = ( res << 6 ) | ( utf8[n] & 0x3F );
	}

	wide = res;

	return cnt;
}

size_t utf8_encode_char( unsigned wide, char * target )
{
	size_t count;

	if ( wide < 0x80 )
		count = 1;
	else if ( wide < 0x800 )
		count = 2;
	else if ( wide < 0x10000 )
		count = 3;
	else if ( wide < 0x200000 )
		count = 4;
	else if ( wide < 0x4000000 )
		count = 5;
	else if ( wide <= 0x7FFFFFFF )
		count = 6;
	else
		return 0;

	if ( target == 0 )
		return count;

	switch ( count )
	{
    case 6:
		target[5] = 0x80 | ( wide & 0x3F );
		wide = wide >> 6;
		wide |= 0x4000000;
    case 5:
		target[4] = 0x80 | ( wide & 0x3F );
		wide = wide >> 6;
		wide |= 0x200000;
    case 4:
		target[3] = 0x80 | ( wide & 0x3F );
		wide = wide >> 6;
		wide |= 0x10000;
    case 3:
		target[2] = 0x80 | ( wide & 0x3F );
		wide = wide >> 6;
		wide |= 0x800;
    case 2:
		target[1] = 0x80 | ( wide & 0x3F );
		wide = wide >> 6;
		wide |= 0xC0;
	case 1:
		target[0] = wide;
	}

	return count;
}

size_t utf16_encode_char( unsigned cur_wchar, blargg_wchar_t * out )
{
	if ( cur_wchar < 0x10000 )
	{
        if ( out ) *out = (blargg_wchar_t) cur_wchar; return 1;
	}
	else if ( cur_wchar < ( 1 << 20 ) )
	{
		unsigned c = cur_wchar - 0x10000;
		//MSDN:
        //The first (high) surrogate is a 16-bit code value in the range U+D800 to U+DBFF. The second (low) surrogate is a 16-bit code value in the range U+DC00 to U+DFFF. Using surrogates, Unicode can support over one million characters. For more details about surrogates, refer to The Unicode Standard, version 2.0.
		if ( out )
		{
            out[0] = ( blargg_wchar_t )( 0xD800 | ( 0x3FF & ( c >> 10 ) ) );
            out[1] = ( blargg_wchar_t )( 0xDC00 | ( 0x3FF & c ) ) ;
		}
		return 2;
	}
	else
	{
		if ( out ) *out = '?'; return 1;
	}
}

size_t utf16_decode_char( const blargg_wchar_t * p_source, unsigned * p_out, size_t p_source_length )
{
	if ( p_source_length == 0 ) return 0;
	else if ( p_source_length == 1 )
	{
		*p_out = p_source[0];
		return 1;
	}
	else
	{
		size_t retval = 0;
		unsigned decoded = p_source[0];
		if ( decoded != 0 )
		{
			retval = 1;
			if ( ( decoded & 0xFC00 ) == 0xD800 )
			{
				unsigned low = p_source[1];
				if ( ( low & 0xFC00 ) == 0xDC00 )
				{
					decoded = 0x10000 + ( ( ( decoded & 0x3FF ) << 10 ) | ( low & 0x3FF ) );
					retval = 2;
				}
			}
		}
		*p_out = decoded;
		return retval;
	}
}

// Converts wide-character path to UTF-8. Free result with free(). Only supported on Windows.
char* blargg_to_utf8( const blargg_wchar_t* wpath )
{
	if ( wpath == NULL )
		return NULL;

	size_t needed = 0;
    size_t mmax = blargg_wcslen( wpath );
	if ( mmax <= 0 )
		return NULL;

	size_t ptr = 0;
	while ( ptr < mmax )
	{
		unsigned wide = 0;
		size_t char_len = utf16_decode_char( wpath + ptr, &wide, mmax - ptr );
		if ( !char_len ) break;
		ptr += char_len;
		needed += utf8_encode_char( wide, 0 );
	}
	if ( needed <= 0 )
		return NULL;

	char* path = (char*) calloc( needed + 1, 1 );
	if ( path == NULL )
		return NULL;

	ptr = 0;
	size_t actual = 0;
	while ( ptr < mmax && actual < needed )
	{
		unsigned wide = 0;
		size_t char_len = utf16_decode_char( wpath + ptr, &wide, mmax - ptr );
		if ( !char_len ) break;
		ptr += char_len;
		actual += utf8_encode_char( wide, path + actual );
	}

	if ( actual == 0 )
	{
		free( path );
		return NULL;
	}

	assert( actual == needed );
	return path;
}

// Converts UTF-8 path to wide-character. Free result with free() Only supported on Windows.
blargg_wchar_t* blargg_to_wide( const char* path )
{
	if ( path == NULL )
		return NULL;

	size_t mmax = strlen( path );
	if ( mmax <= 0 )
		return NULL;

	size_t needed = 0;
	size_t ptr = 0;
	while ( ptr < mmax )
	{
		unsigned wide = 0;
		size_t char_len = utf8_decode_char( path + ptr, wide, mmax - ptr );
		if ( !char_len ) break;
		ptr += char_len;
		needed += utf16_encode_char( wide, 0 );
	}
	if ( needed <= 0 )
		return NULL;

    blargg_wchar_t* wpath = (blargg_wchar_t*) calloc( needed + 1, sizeof *wpath );
	if ( wpath == NULL )
		return NULL;

	ptr = 0;
	size_t actual = 0;
	while ( ptr < mmax && actual < needed )
	{
		unsigned wide = 0;
		size_t char_len = utf8_decode_char( path + ptr, wide, mmax - ptr );
		if ( !char_len ) break;
		ptr += char_len;
		actual += utf16_encode_char( wide, wpath + actual );
	}
	if ( actual == 0 )
	{
		free( wpath );
		return NULL;
	}

	assert( actual == needed );
	return wpath;
}

BLARGG_NAMESPACE_END
