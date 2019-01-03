// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ay_Core.h"

/* Copyright (C) 2006-2009 Shay Green. This module is free software; you
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

inline void Ay_Core::disable_beeper()
{
	beeper_mask = 0;
	last_beeper = 0;
}

Ay_Core::Ay_Core()
{
	beeper_output = NULL;
	disable_beeper();
}

Ay_Core::~Ay_Core() { }

void Ay_Core::set_beeper_output( Blip_Buffer* b )
{
	beeper_output = b;
	if ( b && !cpc_mode )
		beeper_mask = 0x10;
	else
		disable_beeper();
}

void Ay_Core::start_track( registers_t const& r, addr_t play )
{
	play_addr = play;
	
	memset( mem_.padding1, 0xFF, sizeof mem_.padding1 );
	
	int const mirrored = 0x80; // this much is mirrored after end of memory
	memset( mem_.ram + mem_size + mirrored, 0xFF, sizeof mem_.ram - mem_size - mirrored );
	memcpy( mem_.ram + mem_size, mem_.ram, mirrored ); // some code wraps around (ugh)
	
	cpu.reset( mem_.padding1, mem_.padding1 );
	cpu.map_mem( 0, mem_size, mem_.ram, mem_.ram );
	cpu.r = r;
	
	beeper_delta   = (int) (apu_.amp_range * 0.8);
	last_beeper    = 0;
	next_play      = play_period;
	spectrum_mode  = false;
	cpc_mode       = false;
	cpc_latch      = 0;
	set_beeper_output( beeper_output );
	apu_.reset();
	
	// a few tunes rely on channels having tone enabled at the beginning
	apu_.write_addr( 7 );
	apu_.write_data( 0, 0x38 );
	
}

// Emulation

void Ay_Core::cpu_out_( time_t time, addr_t addr, int data )
{
	// Spectrum
	if ( !cpc_mode )
	{
		switch ( addr & 0xFEFF )
		{
		case 0xFEFD:
			spectrum_mode = true;
			apu_.write_addr( data );
			return;
		
		case 0xBEFD:
			spectrum_mode = true;
			apu_.write_data( time, data );
			return;
		}
	}
	
	// CPC
	if ( !spectrum_mode )
	{
		switch ( addr >> 8 )
		{
		case 0xF6:
			switch ( data & 0xC0 )
			{
			case 0xC0:
				apu_.write_addr( cpc_latch );
				goto enable_cpc;
			
			case 0x80:
				apu_.write_data( time, cpc_latch );
				goto enable_cpc;
			}
			break;
		
		case 0xF4:
			cpc_latch = data;
			goto enable_cpc;
		}
	}
	
	dprintf( "Unmapped OUT: $%04X <- $%02X\n", addr, data );
	return;
	
enable_cpc:
	if ( !cpc_mode )
	{
		cpc_mode = true;
		disable_beeper();
		set_cpc_callback.f( set_cpc_callback.data );
	}
}

int Ay_Core::cpu_in( addr_t addr )
{
	// keyboard read and other things
	if ( (addr & 0xFF) == 0xFE )
		return 0xFF; // other values break some beeper tunes
	
	dprintf( "Unmapped IN : $%04X\n", addr );
	return 0xFF;
}

void Ay_Core::end_frame( time_t* end )
{
	cpu.set_time( 0 );
	
	// Since detection of CPC mode will halve clock rate during the frame
	// and thus generate up to twice as much sound, we must generate half
	// as much until mode is known.
	if ( !(spectrum_mode | cpc_mode) )
		*end /= 2;
	
	while ( cpu.time() < *end )
	{
		run_cpu( min( *end, next_play ) );
		
		if ( cpu.time() >= next_play )
		{
			// next frame
			next_play += play_period;
			
			if ( cpu.r.iff1 )
			{
				// interrupt enabled
				
				if ( mem_.ram [cpu.r.pc] == 0x76 )
					cpu.r.pc++; // advance past HALT instruction
				
				cpu.r.iff1 = 0;
				cpu.r.iff2 = 0;
				
				mem_.ram [--cpu.r.sp] = byte (cpu.r.pc >> 8);
				mem_.ram [--cpu.r.sp] = byte (cpu.r.pc);
				
				// fixed interrupt
				cpu.r.pc = 0x38;
				cpu.adjust_time( 12 );
				
				if ( cpu.r.im == 2 )
				{
					// vectored interrupt
					addr_t addr = cpu.r.i * 0x100 + 0xFF;
					cpu.r.pc = mem_.ram [(addr + 1) & 0xFFFF] * 0x100 + mem_.ram [addr];
					cpu.adjust_time( 6 );
				}
			}
		}
	}
	
	// End time frame
	*end = cpu.time();
	next_play -= *end;
	check( next_play >= 0 );
	cpu.adjust_time( -*end );
	apu_.end_frame( *end );
}
