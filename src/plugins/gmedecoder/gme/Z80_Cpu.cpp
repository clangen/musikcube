// $package. http://www.slack.net/~ant/

#include "Z80_Cpu.h"

/* Copyright (C) 2006-2008 Shay Green. This module is free software; you
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

// flags, named with hex value for clarity
int const S80 = 0x80;
int const Z40 = 0x40;
int const F20 = 0x20;
int const H10 = 0x10;
int const F08 = 0x08;
int const V04 = 0x04;
int const P04 = 0x04;
int const N02 = 0x02;
int const C01 = 0x01;

Z80_Cpu::Z80_Cpu()
{
	cpu_state = &cpu_state_;
	
	for ( int i = 0x100; --i >= 0; )
	{
		int even = 1;
		for ( int p = i; p; p >>= 1 )
			even ^= p;
		int n = (i & (S80 | F20 | F08)) | ((even & 1) * P04);
		szpc [i] = n;
		szpc [i + 0x100] = n | C01;
	}
	szpc [0x000] |= Z40;
	szpc [0x100] |= Z40;
}

inline void Z80_Cpu::set_page( int i, void* write, void const* read )
{
	int offset = Z80_CPU_OFFSET( i * page_size );
	byte      * write2 = STATIC_CAST(byte      *,write) - offset;
	byte const* read2  = STATIC_CAST(byte const*,read ) - offset;
	cpu_state_.write [i] = write2;
	cpu_state_.read  [i] = read2;
	cpu_state->write [i] = write2;
	cpu_state->read  [i] = read2;
}

void Z80_Cpu::reset( void* unmapped_write, void const* unmapped_read )
{
	check( cpu_state == &cpu_state_ );
	cpu_state = &cpu_state_;
	cpu_state_.time = 0;
	cpu_state_.base = 0;
	end_time_   = 0;
	
	for ( int i = 0; i < page_count + 1; i++ )
		set_page( i, unmapped_write, unmapped_read );
	
	memset( &r, 0, sizeof r );
}

void Z80_Cpu::map_mem( addr_t start, int size, void* write, void const* read )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size  % page_size == 0 );
	require( start + size <= 0x10000 );
	
	for ( int offset = 0; offset < size; offset += page_size )
		set_page( (start + offset) >> page_bits,
				STATIC_CAST(char      *,write) + offset,
				STATIC_CAST(char const*,read ) + offset );
}
