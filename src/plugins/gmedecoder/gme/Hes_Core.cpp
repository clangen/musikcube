// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Hes_Core.h"

#include "blargg_endian.h"

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

int const timer_mask  = 0x04;
int const vdp_mask    = 0x02;
int const i_flag_mask = 0x04;
int const unmapped    = 0xFF;

int const period_60hz = 262 * 455; // scanlines * clocks per scanline

Hes_Core::Hes_Core() : rom( Hes_Cpu::page_size )
{
	timer.raw_load = 0;
}

Hes_Core::~Hes_Core() { }

void Hes_Core::unload()
{
	rom.clear();
	Gme_Loader::unload();
}

bool Hes_Core::header_t::valid_tag() const
{
	return 0 == memcmp( tag, "HESM", 4 );
}

blargg_err_t Hes_Core::load_( Data_Reader& in )
{
	assert( offsetof (header_t,unused [4]) == header_t::size );
	RETURN_ERR( rom.load( in, header_t::size, &header_, unmapped ) );
	
	if ( !header_.valid_tag() )
		return blargg_err_file_type;
	
	if ( header_.vers != 0 )
		set_warning( "Unknown file version" );
	
	if ( memcmp( header_.data_tag, "DATA", 4 ) )
		set_warning( "Data header missing" );
	
	if ( memcmp( header_.unused, "\0\0\0\0", 4 ) )
		set_warning( "Unknown header data" );
	
	// File spec supports multiple blocks, but I haven't found any, and
	// many files have bad sizes in the only block, so it's simpler to
	// just try to load the damn data as best as possible.
	
	int addr = get_le32( header_.addr );
	int size = get_le32( header_.data_size );
	int const rom_max = 0x100000;
	if ( (unsigned) addr >= (unsigned) rom_max )
	{
		set_warning( "Invalid address" );
		addr &= rom_max - 1;
	}
	if ( (unsigned) (addr + size) > (unsigned) rom_max )
		set_warning( "Invalid size" );
	
	if ( size != rom.file_size() )
	{
		if ( size <= rom.file_size() - 4 && !memcmp( rom.begin() + size, "DATA", 4 ) )
			set_warning( "Multiple DATA not supported" );
		else if ( size < rom.file_size() )
			set_warning( "Extra file data" );
		else
			set_warning( "Missing file data" );
	}
	
	rom.set_addr( addr );
	
	return blargg_ok;
}

void Hes_Core::recalc_timer_load()
{
	timer.load = timer.raw_load * timer_base + 1;
}

void Hes_Core::set_tempo( double t )
{
	play_period = (time_t) (period_60hz / t);
    timer_base = (int) (1024 / t);
	recalc_timer_load();
}

blargg_err_t Hes_Core::start_track( int track )
{
	memset( ram, 0, sizeof ram ); // some HES music relies on zero fill
	memset( sgx, 0, sizeof sgx );
	
	apu_.reset();
	adpcm_.reset();
	cpu.reset();
	
	for ( int i = 0; i < (int) sizeof header_.banks; i++ )
		set_mmr( i, header_.banks [i] );
	set_mmr( cpu.page_count, 0xFF ); // unmapped beyond end of address space
	
	irq.disables  = timer_mask | vdp_mask;
	irq.timer     = cpu.future_time;
	irq.vdp       = cpu.future_time;
	
	timer.enabled   = false;
	timer.raw_load  = 0x80;
	timer.count     = timer.load;
	timer.fired     = false;
	timer.last_time = 0;
	
	vdp.latch     = 0;
	vdp.control   = 0;
	vdp.next_vbl  = 0;
	
	ram [0x1FF] = (idle_addr - 1) >> 8;
	ram [0x1FE] = (idle_addr - 1) & 0xFF;
	cpu.r.sp = 0xFD;
	cpu.r.pc = get_le16( header_.init_addr );
	cpu.r.a  = track;
	
	recalc_timer_load();
	
	return blargg_ok;
}

// Hardware

void Hes_Core::run_until( time_t present )
{
	while ( vdp.next_vbl < present )
		vdp.next_vbl += play_period;
	
	time_t elapsed = present - timer.last_time;
	if ( elapsed > 0 )
	{
		if ( timer.enabled )
		{
			timer.count -= elapsed;
			if ( timer.count <= 0 )
				timer.count += timer.load;
		}
		timer.last_time = present;
	}
}

void Hes_Core::write_vdp( int addr, int data )
{
	switch ( addr )
	{
	case 0:
		vdp.latch = data & 0x1F;
		break;
	
	case 2:
		if ( vdp.latch == 5 )
		{
			if ( data & 0x04 )
				set_warning( "Scanline interrupt unsupported" );
			run_until( cpu.time() );
			vdp.control = data;
			irq_changed();
		}
		else
		{
			dprintf( "VDP not supported: $%02X <- $%02X\n", vdp.latch, data );
		}
		break;
	
	case 3:
		dprintf( "VDP MSB not supported: $%02X <- $%02X\n", vdp.latch, data );
		break;
	}
}

void Hes_Core::write_mem_( addr_t addr, int data )
{
	time_t time = cpu.time();
	if ( (unsigned) (addr - apu_.io_addr) < apu_.io_size )
	{
		// Avoid going way past end when a long block xfer is writing to I/O space.
		// Not a problem for other registers below because they don't write to
		// Blip_Buffer.
		time_t t = min( time, cpu.end_time() + 8 );
        apu_.write_data( t, addr, data );
		return;
	}
	if ( (unsigned) (addr - adpcm_.io_addr) < adpcm_.io_size )
	{
		time_t t = min( time, cpu.end_time() + 6 );
        adpcm_.write_data( t, addr, data );
		return;
	}
	
	switch ( addr )
	{
	case 0x0000:
	case 0x0002:
	case 0x0003:
		write_vdp( addr, data );
		return;
	
	case 0x0C00: {
		run_until( time );
		timer.raw_load = (data & 0x7F) + 1;
		recalc_timer_load();
		timer.count = timer.load;
		break;
	}
	
	case 0x0C01:
		data &= 1;
		if ( timer.enabled == data )
			return;
		run_until( time );
		timer.enabled = data;
		if ( data )
			timer.count = timer.load;
		break;
	
	case 0x1402:
		run_until( time );
		irq.disables = data;
		if ( (data & 0xF8) && (data & 0xF8) != 0xF8 ) // flag questionable values
			dprintf( "Int mask: $%02X\n", data );
		break;
	
	case 0x1403:
		run_until( time );
		if ( timer.enabled )
			timer.count = timer.load;
		timer.fired = false;
		break;
	
#ifndef NDEBUG
	case 0x1000: // I/O port
	case 0x0402: // palette
	case 0x0403:
	case 0x0404:
	case 0x0405:
		return;
		
	default:
		dprintf( "unmapped write $%04X <- $%02X\n", addr, data );
		return;
#endif
	}
	
	irq_changed();
}

int Hes_Core::read_mem_( addr_t addr )
{
	time_t time = cpu.time();
	addr &= cpu.page_size - 1;
	switch ( addr )
	{
	case 0x0000:
		if ( irq.vdp > time )
			return 0;
		irq.vdp = cpu.future_time;
		run_until( time );
		irq_changed();
		return 0x20;
		
	case 0x0002:
	case 0x0003:
		dprintf( "VDP read not supported: %d\n", addr );
		return 0;
	
	case 0x0C01:
		//return timer.enabled; // TODO: remove?
	case 0x0C00:
		run_until( time );
		dprintf( "Timer count read\n" );
		return (unsigned) (timer.count - 1) / timer_base;
	
	case 0x1402:
		return irq.disables;
	
	case 0x1403:
		{
			int status = 0;
			if ( irq.timer <= time ) status |= timer_mask;
			if ( irq.vdp   <= time ) status |= vdp_mask;
			return status;
		}

	case 0x180A:
	case 0x180B:
	case 0x180C:
	case 0x180D:
        return adpcm_.read_data( time, addr );
		
	#ifndef NDEBUG
		case 0x1000: // I/O port
		//case 0x180C: // CD-ROM
		//case 0x180D:
			break;
		
		default:
			dprintf( "unmapped read  $%04X\n", addr );
	#endif
	}
	
	return unmapped;
}

void Hes_Core::irq_changed()
{
	time_t present = cpu.time();
	
	if ( irq.timer > present )
	{
		irq.timer = cpu.future_time;
		if ( timer.enabled && !timer.fired )
			irq.timer = present + timer.count;
	}
	
	if ( irq.vdp > present )
	{
		irq.vdp = cpu.future_time;
		if ( vdp.control & 0x08 )
			irq.vdp = vdp.next_vbl;
	}
	
	time_t time = cpu.future_time;
	if ( !(irq.disables & timer_mask) ) time = irq.timer;
	if ( !(irq.disables &   vdp_mask) ) time = min( time, irq.vdp );
	
	cpu.set_irq_time( time );
}

int Hes_Core::cpu_done()
{
	check( cpu.time() >= cpu.end_time() ||
			(!(cpu.r.flags & i_flag_mask) && cpu.time() >= cpu.irq_time()) );
	
	if ( !(cpu.r.flags & i_flag_mask) )
	{
		time_t present = cpu.time();
		
		if ( irq.timer <= present && !(irq.disables & timer_mask) )
		{
			timer.fired = true;
			irq.timer = cpu.future_time;
			irq_changed(); // overkill, but not worth writing custom code
			return 0x0A;
		}
		
		if ( irq.vdp <= present && !(irq.disables & vdp_mask) )
		{
			// work around for bugs with music not acknowledging VDP
			//run_until( present );
			//irq.vdp = cpu.future_time;
			//irq_changed();
			return 0x08;
		}
	}
	return -1;
}

static void adjust_time( Hes_Core::time_t& time, Hes_Core::time_t delta )
{
	if ( time < Hes_Cpu::future_time )
	{
		time -= delta;
		if ( time < 0 )
			time = 0;
	}
}

blargg_err_t Hes_Core::end_frame( time_t duration )
{
	if ( run_cpu( duration ) )
		set_warning( "Emulation error (illegal instruction)" );
	
	check( cpu.time() >= duration );
	//check( time() - duration < 20 ); // Txx instruction could cause going way over
	
	run_until( duration );
	
	// end time frame
	timer.last_time -= duration;
	vdp.next_vbl    -= duration;
	cpu.end_frame( duration );
	::adjust_time( irq.timer, duration );
	::adjust_time( irq.vdp,   duration );
    apu_.end_frame( duration );
    adpcm_.end_frame( duration );
	
	return blargg_ok;
}
