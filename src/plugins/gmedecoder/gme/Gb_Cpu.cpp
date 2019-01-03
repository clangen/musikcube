// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gb_Cpu.h"

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

inline void Gb_Cpu::set_code_page( int i, void* p )
{
	byte* p2 = STATIC_CAST(byte*,p) - GB_CPU_OFFSET( i * page_size );
	cpu_state_.code_map [i] = p2;
	cpu_state->code_map [i] = p2;
}

void Gb_Cpu::reset( void* unmapped )
{
	check( cpu_state == &cpu_state_ );
	cpu_state = &cpu_state_;
	
	cpu_state_.time = 0;
	
	for ( int i = 0; i < page_count + 1; ++i )
		set_code_page( i, unmapped );
	
	memset( &r, 0, sizeof r );
	
	blargg_verify_byte_order();
}

void Gb_Cpu::map_code( addr_t start, int size, void* data )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size  % page_size == 0 );
	require( start + size <= mem_size );
	
	for ( int offset = 0; offset < size; offset += page_size )
		set_code_page( (start + offset) >> page_bits, STATIC_CAST(char*,data) + offset );
}
