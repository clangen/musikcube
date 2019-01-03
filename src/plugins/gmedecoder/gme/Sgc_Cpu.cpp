// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sgc_Impl.h"

#include "blargg_endian.h"
//#include "z80_cpu_log.h"

/* Copyright (C) 2009 Shay Green. This module is free software; you
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

#define OUT_PORT(  addr, data ) cpu_out( TIME(), addr, data )
#define IN_PORT(   addr       ) cpu_in( addr )
#define WRITE_MEM( addr, data ) cpu_write( addr, data )
#define IDLE_ADDR               idle_addr
#define CPU                     cpu
#define RST_BASE                vectors_addr

#define CPU_BEGIN \
bool Sgc_Impl::run_cpu( time_t end_time )\
{\
	cpu.set_end_time( end_time );

	#include "Z80_Cpu_run.h"
	
	return warning;
}
