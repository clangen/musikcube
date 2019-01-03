// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sap_Core.h"

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

int const idle_addr = 0xD2D2;

Sap_Core::Sap_Core()
{
	set_tempo( 1 );
}

void Sap_Core::push( int b )
{
	mem.ram [0x100 + cpu.r.sp--] = (byte) b;
}

void Sap_Core::jsr_then_stop( addr_t addr )
{
	cpu.r.pc = addr;
	
	// Some rips pop three bytes off stack before RTS.
	push( (idle_addr - 1) >> 8 );
	push(  idle_addr - 1       );
	
	// 3 bytes so that RTI or RTS will jump to idle_addr.
	// RTI will use the first two bytes as the address, 0xD2D2.
	// RTS will use the last two bytes, 0xD2D1, which it internally increments.
	push( (idle_addr - 1) >> 8 );
	push( (idle_addr - 1) >> 8 );
	push(  idle_addr - 1       );
}

// Runs routine and allows it up to one second to return
void Sap_Core::run_routine( addr_t addr )
{
	jsr_then_stop( addr );
	run_cpu( lines_per_frame * base_scanline_period * 60 );
	check( cpu.r.pc == idle_addr );
	check( cpu.r.sp >= 0xFF - 6 );
}

inline void Sap_Core::call_init( int track )
{
	cpu.r.a = track;
	
	switch ( info.type )
	{
	case 'B':
		run_routine( info.init_addr );
		break;
	
	case 'C':
		cpu.r.a = 0x70;
		cpu.r.x = info.music_addr&0xFF;
		cpu.r.y = info.music_addr >> 8;
		run_routine( info.play_addr + 3 );
		cpu.r.a = 0;
		cpu.r.x = track;
		run_routine( info.play_addr + 3 );
		break;
	
	case 'D':
		check( info.fastplay == lines_per_frame );
		jsr_then_stop( info.init_addr );
		break;
	}
}

void Sap_Core::setup_ram()
{
	memset( &mem, 0, sizeof mem );
	
	ram() [idle_addr] = cpu.halt_opcode;
	
	addr_t const irq_addr = idle_addr - 1;
	ram() [irq_addr] = cpu.halt_opcode;
	ram() [0xFFFE] = (byte) irq_addr;
	ram() [0xFFFF] = irq_addr >> 8;
}

blargg_err_t Sap_Core::start_track( int track, info_t const& new_info )
{
	info = new_info;
	
	check( ram() [idle_addr] == cpu.halt_opcode );
	
	apu_ .reset( &apu_impl_ );
	apu2_.reset( &apu_impl_ );
	
	cpu.reset( ram() );
	
	frame_start = 0;
	next_play = play_period() * 4;
	saved_state.pc = idle_addr;
	
	time_mask = 0; // disables sound during init
	call_init( track );
	time_mask = ~0;
	
	return blargg_ok;
}

blargg_err_t Sap_Core::run_until( time_t end )
{
	while ( cpu.time() < end )
	{
		time_t next = min( next_play, end );
		if ( (run_cpu( next ) && cpu.r.pc != idle_addr) || cpu.error_count() )
			// TODO: better error
			return BLARGG_ERR( BLARGG_ERR_GENERIC, "Emulation error (illegal instruction)" );
		
		if ( cpu.r.pc == idle_addr )
		{
			if ( saved_state.pc == idle_addr )
			{
				// no code to run until next play call
				cpu.set_time( next );
			}
			else
			{
				// play had interrupted non-returning init, so restore registers
				// init routine was running
				check( cpu.r.sp == saved_state.sp - 3 );
				cpu.r = saved_state;
				saved_state.pc = idle_addr;
			}
		}
		
		if ( cpu.time() >= next_play )
		{
			next_play += play_period();
			
			if ( cpu.r.pc == idle_addr || info.type == 'D' )
			{
				// Save state if init routine is still running
				if ( cpu.r.pc != idle_addr )
				{
					check( info.type == 'D' );
					check( saved_state.pc == idle_addr );
					saved_state = cpu.r;
				}
				
				addr_t addr = info.play_addr;
				if ( info.type == 'C' )
					addr += 6;
				jsr_then_stop( addr );
			}
			else
			{
				dprintf( "init/play hadn't returned before next play call\n" );
			}
		}
	}
	return blargg_ok;
}

blargg_err_t Sap_Core::end_frame( time_t end )
{
	RETURN_ERR( run_until( end ) );
	
	cpu.adjust_time( -end );
	
	time_t frame_time = lines_per_frame * scanline_period;
	while ( frame_start < end )
		frame_start += frame_time;
	frame_start -= end + frame_time;
	
	if ( (next_play -= end) < 0 )
	{
		next_play = 0;
		check( false );
	}
	
	apu_.end_frame( end );
	if ( info.stereo )
		apu2_.end_frame( end );
	
	return blargg_ok;
}
