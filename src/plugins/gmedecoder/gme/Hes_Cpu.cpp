// $package. http://www.slack.net/~ant/

#include "Hes_Cpu.h"

#include "blargg_endian.h"
#include "Hes_Core.h"

//#include "hes_cpu_log.h"

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

#define PAGE HES_CPU_PAGE

int Hes_Core::read_mem( addr_t addr )
{
	check( addr < 0x10000 );
	int result = *cpu.get_code( addr );
	if ( cpu.mmr [PAGE( addr )] == 0xFF )
		result = read_mem_( addr );
	return result;
}

void Hes_Core::write_mem( addr_t addr, int data )
{
	check( addr < 0x10000 );
	byte* out = write_pages [PAGE( addr )];
	if ( out )
		out [addr & (cpu.page_size - 1)] = data;
	else if ( cpu.mmr [PAGE( addr )] == 0xFF )
		write_mem_( addr, data );
}

void Hes_Core::set_mmr( int page, int bank )
{
	write_pages [page] = 0;
	byte* data = rom.at_addr( bank * cpu.page_size );
	if ( bank >= 0x80 )
	{
		data = 0;
		switch ( bank )
		{
		case 0xF8:
			data = ram;
			break;
		
		case 0xF9:
		case 0xFA:
		case 0xFB:
			data = &sgx [(bank - 0xF9) * cpu.page_size];
			break;
		
		default:
			if ( bank != 0xFF )
				dprintf( "Unmapped bank $%02X\n", bank );
			data = rom.unmapped();
			goto end;
		}
		
		write_pages [page] = data;
	}
end:
	cpu.set_mmr( page, bank, data );
}

#define READ_FAST( addr, out ) \
{\
	out = READ_CODE( addr );\
	if ( CPU.mmr [PAGE( addr )] == 0xFF )\
	{\
		FLUSH_TIME();\
		out = read_mem_( addr );\
		CACHE_TIME();\
	}\
}

#define WRITE_FAST( addr, data ) \
{\
	int page = PAGE( addr );\
	byte* out = write_pages [page];\
	addr &= CPU.page_size - 1;\
	if ( out )\
	{\
		out [addr] = data;\
	}\
	else if ( CPU.mmr [page] == 0xFF )\
	{\
		FLUSH_TIME();\
		write_mem_( addr, data );\
		CACHE_TIME();\
	}\
}

#define READ_LOW(  addr )           (ram [addr])
#define WRITE_LOW( addr, data )     (ram [addr] = data)
#define READ_MEM(  addr )           read_mem(  addr )
#define WRITE_MEM( addr, data )     write_mem( addr, data )
#define WRITE_VDP( addr, data )     write_vdp( addr, data )
#define CPU_DONE( result_out )      { FLUSH_TIME(); result_out = cpu_done(); CACHE_TIME(); }
#define SET_MMR( reg, bank )        set_mmr( reg, bank )

#define CPU         cpu
#define IDLE_ADDR   idle_addr

#define CPU_BEGIN \
bool Hes_Core::run_cpu( time_t end_time )\
{\
	cpu.set_end_time( end_time );
	
	#include "Hes_Cpu_run.h"
	
	return illegal_encountered;
}
