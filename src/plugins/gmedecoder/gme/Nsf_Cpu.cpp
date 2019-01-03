// Normal CPU for NSF emulator

// $package. http://www.slack.net/~ant/

#include "Nsf_Impl.h"

#include "blargg_endian.h"

#ifdef BLARGG_DEBUG_H
	//#define CPU_LOG_START 1000000
	//#include "nes_cpu_log.h"
	#undef LOG_MEM
#endif

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

#ifndef LOG_MEM
	#define LOG_MEM( addr, str, data ) data
#endif

int Nsf_Impl::read_mem( addr_t addr )
{
	int result = low_ram [addr & (low_ram_size-1)]; // also handles wrap-around
	if ( addr & 0xE000 )
	{
		result = *cpu.get_code( addr );
		if ( addr < sram_addr )
		{
			if ( addr == apu.status_addr )
				result = apu.read_status( time() );
			else
				result = cpu_read( addr );
		}
	}
	return LOG_MEM( addr, ">", result );
}

void Nsf_Impl::write_mem( addr_t addr, int data )
{
	(void) LOG_MEM( addr, "<", data );
	
	int offset = addr - sram_addr;
	if ( (unsigned) offset < sram_size )
	{
		sram() [offset] = data;
	}
	else
	{
		// after sram because CPU handles most low_ram accesses internally already
		int temp = addr & (low_ram_size-1); // also handles wrap-around
		if ( !(addr & 0xE000) )
		{
			low_ram [temp] = data;
		}
		else
		{
			int bank = addr - banks_addr;
			if ( (unsigned) bank < bank_count )
			{
				write_bank( bank, data );
			}
			else if ( (unsigned) (addr - apu.io_addr) < apu.io_size )
			{
				apu.write_register( time(), addr, data );
			}
			else
			{
			#if !NSF_EMU_APU_ONLY
				// 0x8000-0xDFFF is writable
				int i = addr - 0x8000;
				if ( (unsigned) i < fdsram_size && fds_enabled() )
					fdsram() [i] = data;
				else
			#endif
				cpu_write( addr, data );
			}
		}
	}
}

#define READ_LOW(  addr       ) (LOG_MEM( addr, ">", low_ram [addr] ))
#define WRITE_LOW( addr, data ) (LOG_MEM( addr, "<", low_ram [addr] = data ))

#define CAN_WRITE_FAST( addr )  (addr < low_ram_size)
#define WRITE_FAST              WRITE_LOW

// addr < 0x2000 || addr >= 0x8000
#define CAN_READ_FAST( addr )   ((addr ^ 0x8000) < 0xA000)
#define READ_FAST( addr, out  ) (LOG_MEM( addr, ">", out = READ_CODE( addr ) ))

#define READ_MEM(  addr       ) read_mem(  addr )
#define WRITE_MEM( addr, data ) write_mem( addr, data )

#define CPU cpu

#define CPU_BEGIN \
bool Nsf_Impl::run_cpu_until( time_t end )\
{\
	cpu.set_end_time( end );\
	if ( *cpu.get_code( cpu.r.pc ) != cpu.halt_opcode )\
	{
		#include "Nes_Cpu_run.h"
	}
	return cpu.time_past_end() < 0;
}
