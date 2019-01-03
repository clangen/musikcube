// $package. http://www.slack.net/~ant/

#include "Ay_Core.h"

#include "blargg_endian.h"
//#include "z80_cpu_log.h"

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

void Ay_Core::cpu_out( time_t time, addr_t addr, int data )
{
	if ( (addr & 0xFF) == 0xFE )
	{
		check( !cpc_mode );
		spectrum_mode = !cpc_mode;
		
		// beeper_mask and last_beeper are 0 if (cpc_mode || !beeper_output)
		if ( (data &= beeper_mask) != last_beeper )
		{
			last_beeper = data;
			int delta = -beeper_delta;
			beeper_delta = delta;
			Blip_Buffer* bb = beeper_output;
			bb->set_modified();
			apu_.synth_.offset( time, delta, bb );
		}
	}
	else
	{
		cpu_out_( time, addr, data );
	}
}

#define OUT_PORT( addr, data )  cpu_out( TIME(), addr, data )
#define IN_PORT(  addr       )  cpu_in( addr )
#define FLAT_MEM                mem
#define CPU                     cpu

#define CPU_BEGIN \
bool Ay_Core::run_cpu( time_t end_time ) \
{\
	cpu.set_end_time( end_time );\
	byte* const mem = mem_.ram; // cache
	
	#include "Z80_Cpu_run.h"
	
	return warning;
}
