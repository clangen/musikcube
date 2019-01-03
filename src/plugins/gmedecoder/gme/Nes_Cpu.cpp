// $package. http://www.slack.net/~ant/

#include "Nes_Cpu.h"

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

inline void Nes_Cpu::set_code_page( int i, void const* p )
{
	byte const* p2 = STATIC_CAST(byte const*,p) - NES_CPU_OFFSET( i * page_size );
	cpu_state->code_map [i] = p2;
	cpu_state_.code_map [i] = p2;
}

void Nes_Cpu::map_code( addr_t start, int size, void const* data, int mirror_size )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size  % page_size == 0 );
	require( start + size <= 0x10000 );
	require( mirror_size % page_size == 0 );
	
	for ( int offset = 0; offset < size; offset += page_size )
		set_code_page( NES_CPU_PAGE( start + offset ),
				STATIC_CAST(char const*,data) + (offset & ((unsigned) mirror_size - 1)) );
}

void Nes_Cpu::reset( void const* unmapped_page )
{
	check( cpu_state == &cpu_state_ );
	cpu_state = &cpu_state_;
	
	r.flags = irq_inhibit_mask;
	r.sp = 0xFF;
	r.pc = 0;
	r.a  = 0;
	r.x  = 0;
	r.y  = 0;
	
	cpu_state_.time = 0;
	cpu_state_.base = 0;
	irq_time_ = future_time;
	end_time_ = future_time;
	error_count_ = 0;
	
	set_code_page( page_count, unmapped_page );
	map_code( 0, 0x10000, unmapped_page, page_size );
	
	blargg_verify_byte_order();
}
