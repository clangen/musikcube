// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Rom_Data.h"

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

void Rom_Data::clear()
{
	file_size_ = 0;
	rom_addr   = 0;
	mask       = 0;
	rom.clear();
}

Rom_Data::Rom_Data( int page_size ) :
	pad_size( page_size + pad_extra )
{
	// page_size should be power of 2
	check( (page_size & (page_size - 1)) == 0 );
	
	clear();
}

Rom_Data::~Rom_Data()
{ }

// Reads file into array, placing file_offset bytes of padding before the beginning, and pad_size after the end
blargg_err_t Rom_Data::load_( Data_Reader& in, int header_size, int file_offset )
{
	clear();
	file_size_ = in.remain();
	if ( file_size_ <= header_size ) // <= because there must be data after header
		return blargg_err_file_type;
	
	RETURN_ERR( rom.resize( file_offset + file_size_ + pad_size ) );
	
	return in.read( rom.begin() + file_offset, file_size_ );
}

blargg_err_t Rom_Data::load( Data_Reader& in, int header_size,
		void* header_out, int fill )
{
	int file_offset = pad_size - header_size;
	blargg_err_t err = load_( in, header_size, file_offset );
	if ( err )
	{
		clear();
		return err;
	}
	
	file_size_ -= header_size;
	memcpy( header_out, &rom [file_offset], header_size );
	
	memset( rom.begin()         , fill, pad_size );
	memset( rom.end() - pad_size, fill, pad_size );
	
	return blargg_ok;
}

void Rom_Data::set_addr( int addr )
{
	int const page_size = pad_size - pad_extra;
	
	// Minimum size that contains all bytes and is a multiple of page_size
	int const size = (addr + file_size_ + page_size - 1) / page_size * page_size;
	
	// Find lowest power of 2 that is >= size
	int power2 = 1;
	while ( power2 < size )
		power2 *= 2;
	
	mask = power2 - 1;
	
	// Address of first byte of ROM (possibly negative)
	rom_addr = addr - page_size - pad_extra;

	if ( rom.resize( size - rom_addr + pad_extra ) ) { } // OK if shrink fails
}

byte* Rom_Data::at_addr( int addr )
{
	int offset = mask_addr( addr ) - rom_addr;
	
	if ( (unsigned) offset > (unsigned) (rom.size() - pad_size) )
		offset = 0; // unmapped
	
	return &rom [offset];
}
