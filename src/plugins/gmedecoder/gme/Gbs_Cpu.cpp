// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gbs_Core.h"

#include "blargg_endian.h"

//#include "gb_cpu_log.h"

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

#ifndef LOG_MEM
	#define LOG_MEM( addr, str, data ) data
#endif

int Gbs_Core::read_mem( addr_t addr )
{
	int result = *cpu.get_code( addr );
	if ( (unsigned) (addr - apu_.io_addr) < apu_.io_size )
		result = apu_.read_register( time(), addr );
	
#ifndef NDEBUG
	else if ( unsigned (addr - 0x8000) < 0x2000 || unsigned (addr - 0xE000) < 0x1F00 )
		dprintf( "Unmapped read $%04X\n", (unsigned) addr );
	else if ( unsigned (addr - 0xFF01) < 0xFF80 - 0xFF01 && addr != 0xFF70 && addr != 0xFF05 )
		dprintf( "Unmapped read $%04X\n", (unsigned) addr );
#endif

	return LOG_MEM( addr, ">", result );
}

inline void Gbs_Core::write_io_inline( int offset, int data, int base )
{
	if ( (unsigned) (offset - (apu_.io_addr - base)) < apu_.io_size )
		apu_.write_register( time(), offset + base, data & 0xFF );
	else if ( (unsigned) (offset - (0xFF06 - base)) < 2 )
		update_timer();
	else if ( offset == io_base - base )
		ram [base - ram_addr + offset] = 0; // keep joypad return value 0
	else
		ram [base - ram_addr + offset] = 0xFF;
	
	//if ( offset == 0xFFFF - base )
	//  dprintf( "Wrote interrupt mask\n" );
}

void Gbs_Core::write_mem( addr_t addr, int data )
{
	(void) LOG_MEM( addr, "<", data );
	
	int offset = addr - ram_addr;
	if ( (unsigned) offset < 0x10000 - ram_addr )
	{
		ram [offset] = data;
		
		offset -= 0xE000 - ram_addr;
		if ( (unsigned) offset < 0x1F80 )
			write_io_inline( offset, data, 0xE000 );
	}
	else if ( (unsigned) (offset - (0x2000 - ram_addr)) < 0x2000 )
	{
		set_bank( data & 0xFF );
	}
#ifndef NDEBUG
	else if ( unsigned (addr - 0x8000) < 0x2000 || unsigned (addr - 0xE000) < 0x1F00 )
	{
		dprintf( "Unmapped write $%04X\n", (unsigned) addr );
	}
#endif
}

void Gbs_Core::write_io_( int offset, int data )
{
	write_io_inline( offset, data, io_base );
}

inline void Gbs_Core::write_io( int offset, int data )
{
	(void) LOG_MEM( offset + io_base, "<", data );
	
	ram [io_base - ram_addr + offset] = data;
	if ( (unsigned) offset < 0x80 )
		write_io_( offset, data );
}

int Gbs_Core::read_io( int offset )
{
	int const io_base = 0xFF00;
	int result = ram [io_base - ram_addr + offset];
	
	if ( (unsigned) (offset - (apu_.io_addr - io_base)) < apu_.io_size )
	{
		result = apu_.read_register( time(), offset + io_base );
		(void) LOG_MEM( offset + io_base, ">", result );
	}
	else
	{
		check( result == read_mem( offset + io_base ) );
	}
	return result;
}

#define READ_FAST( addr, out ) \
{\
	out = READ_CODE( addr );\
	if ( (unsigned) (addr - apu_.io_addr) < apu_.io_size )\
		out = LOG_MEM( addr, ">", apu_.read_register( TIME() + end_time, addr ) );\
	else\
		check( out == read_mem( addr ) );\
}

#define READ_MEM(  addr       ) read_mem( addr )
#define WRITE_MEM( addr, data ) write_mem( addr, data )

#define WRITE_IO( addr, data )  write_io( addr, data )
#define READ_IO( addr, out )    out = read_io( addr )

#define CPU cpu

#define CPU_BEGIN \
void Gbs_Core::run_cpu()\
{
	#include "Gb_Cpu_run.h"
}
