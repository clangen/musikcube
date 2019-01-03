// $package. http://www.slack.net/~ant/

#include "Sap_Core.h"

#include "blargg_endian.h"

//#define CPU_LOG_MAX 100000
//#include "nes_cpu_log.h"

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

// functions defined in same file as CPU emulator to help compiler's optimizer

int Sap_Core::read_d40b()
{
	//dprintf( "D40B read\n" );
	check( cpu.time() >= frame_start );
	return ((unsigned) (cpu.time() - frame_start) / scanline_period % lines_per_frame) / 2;
}

void Sap_Core::write_D2xx( int d2xx, int data )
{
	addr_t const base = 0xD200;
	
	if ( d2xx < apu_.io_size )
	{
		apu_.write_data( time(), d2xx + base, data );
		return;
	}
	
	if ( (unsigned) (d2xx - 0x10) < apu2_.io_size && info.stereo )
	{
		apu2_.write_data( time(), d2xx + (base - 0x10), data );
		return;
	}
	
	if ( d2xx == 0xD40A - base )
	{
		dprintf( "D40A write\n" );
		time_t t = cpu.time();
		time_t into_line = (t - frame_start) % scanline_period;
		cpu.set_end_time( t - into_line + scanline_period );
		return;
	}
	
	if ( (d2xx & ~0x0010) != 0x0F || data != 0x03 )
		dprintf( "Unmapped write $%04X <- $%02X\n", d2xx + base, data );
}

inline int Sap_Core::read_mem( addr_t addr )
{
	int result = mem.ram [addr];
	if ( addr == 0xD40B )
		result = read_d40b(); 
	else if ( (addr & 0xF900) == 0xD000 )
		dprintf( "Unmapped read $%04X\n", addr );
	return result;
}


#define READ_LOW(  addr       ) (ram [addr])
#define WRITE_LOW( addr, data ) (ram [addr] = data)

#define READ_MEM(  addr       ) read_mem( addr )
#define WRITE_MEM( addr, data ) \
{\
	ram [addr] = data;\
	int d2xx = addr - 0xD200;\
	if ( (unsigned) d2xx < 0x100 )\
		write_D2xx( d2xx, data );\
}

#define CPU         cpu
#define FLAT_MEM    ram

#define CPU_BEGIN \
bool Sap_Core::run_cpu( time_t end )\
{\
	CPU.set_end_time( end );\
	byte* const ram = this->mem.ram; /* cache */
	
	#include "Nes_Cpu_run.h"
	
	return cpu.time_past_end() < 0;
}
