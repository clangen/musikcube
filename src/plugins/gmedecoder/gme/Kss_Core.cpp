// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Kss_Core.h"

#include "blargg_endian.h"

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

Kss_Core::Kss_Core() : rom( Kss_Cpu::page_size )
{
	memset( unmapped_read, 0xFF, sizeof unmapped_read );
}

Kss_Core::~Kss_Core() { }

void Kss_Core::unload()
{
	rom.clear();
}

static blargg_err_t check_kss_header( void const* header )
{
	if ( memcmp( header, "KSCC", 4 ) && memcmp( header, "KSSX", 4 ) )
		return blargg_err_file_type;
	return blargg_ok;
}

blargg_err_t Kss_Core::load_( Data_Reader& in )
{
	memset( &header_, 0, sizeof header_ );
	assert( offsetof (header_t,msx_audio_vol) == header_t::size - 1 );
	RETURN_ERR( rom.load( in, header_t::base_size, &header_, 0 ) );
	
	RETURN_ERR( check_kss_header( header_.tag ) );
	
	header_.last_track [0] = 255;
	if ( header_.tag [3] == 'C' )
	{
		if ( header_.extra_header )
		{
			header_.extra_header = 0;
			set_warning( "Unknown data in header" );
		}
		if ( header_.device_flags & ~0x0F )
		{
			header_.device_flags &= 0x0F;
			set_warning( "Unknown data in header" );
		}
	}
	else if ( header_.extra_header )
	{
		if ( header_.extra_header != header_.ext_size )
		{
			header_.extra_header = 0;
			set_warning( "Invalid extra_header_size" );
		}
		else
		{
			memcpy( header_.data_size, rom.begin(), header_.ext_size );
		}
	}
	
	#ifndef NDEBUG
	{
		int ram_mode = header_.device_flags & 0x84; // MSX
		if ( header_.device_flags & 0x02 ) // SMS
			ram_mode = (header_.device_flags & 0x88);
		
		if ( ram_mode )
			dprintf( "RAM not supported\n" ); // TODO: support
	}
	#endif
	
	return blargg_ok;
}

void Kss_Core::jsr( byte const (&addr) [2] )
{
	ram [--cpu.r.sp] = idle_addr >> 8;
	ram [--cpu.r.sp] = idle_addr & 0xFF;
	cpu.r.pc = get_le16( addr );
}

blargg_err_t Kss_Core::start_track( int track )
{
	memset( ram, 0xC9, 0x4000 );
	memset( ram + 0x4000, 0, sizeof ram - 0x4000 );
	
	// copy driver code to lo RAM
	static byte const bios [] = {
		0xD3, 0xA0, 0xF5, 0x7B, 0xD3, 0xA1, 0xF1, 0xC9, // $0001: WRTPSG
		0xD3, 0xA0, 0xDB, 0xA2, 0xC9                    // $0009: RDPSG
	};
	static byte const vectors [] = {
		0xC3, 0x01, 0x00,   // $0093: WRTPSG vector
		0xC3, 0x09, 0x00,   // $0096: RDPSG vector
	};
	memcpy( ram + 0x01, bios,    sizeof bios );
	memcpy( ram + 0x93, vectors, sizeof vectors );
	
	// copy non-banked data into RAM
	int load_addr = get_le16( header_.load_addr );
	int orig_load_size = get_le16( header_.load_size );
	int load_size = min( orig_load_size, rom.file_size() );
	load_size = min( load_size, (int) mem_size - load_addr );
	if ( load_size != orig_load_size )
		set_warning( "Excessive data size" );
	memcpy( ram + load_addr, rom.begin() + header_.extra_header, load_size );
	
	rom.set_addr( -load_size - header_.extra_header );
	
	// check available bank data
	int const bank_size = this->bank_size();
	int max_banks = (rom.file_size() - load_size + bank_size - 1) / bank_size;
	bank_count = header_.bank_mode & 0x7F;
	if ( bank_count > max_banks )
	{
		bank_count = max_banks;
		set_warning( "Bank data missing" );
	}
	//dprintf( "load_size : $%X\n", load_size );
	//dprintf( "bank_size : $%X\n", bank_size );
	//dprintf( "bank_count: %d (%d claimed)\n", bank_count, header_.bank_mode & 0x7F );
	
	ram [idle_addr] = 0xFF;
	cpu.reset( unmapped_write, unmapped_read );
	cpu.map_mem( 0, mem_size, ram, ram );
	
	cpu.r.sp = 0xF380;
	cpu.r.b.a = track;
	cpu.r.b.h = 0;
	next_play = play_period;
	gain_updated = false;
	jsr( header_.init_addr );
	
	return blargg_ok;
}

void Kss_Core::set_bank( int logical, int physical )
{
	int const bank_size = this->bank_size();
	
	int addr = 0x8000;
	if ( logical && bank_size == 8 * 1024 )
		addr = 0xA000;
	
	physical -= header_.first_bank;
	if ( (unsigned) physical >= (unsigned) bank_count )
	{
		byte* data = ram + addr;
		cpu.map_mem( addr, bank_size, data, data );
	}
	else
	{
		int phys = physical * bank_size;
		for ( int offset = 0; offset < bank_size; offset += cpu.page_size )
			cpu.map_mem( addr + offset, cpu.page_size,
					unmapped_write, rom.at_addr( phys + offset ) );
	}
}

void Kss_Core::cpu_out( time_t, addr_t addr, int data )
{
	dprintf( "OUT $%04X,$%02X\n", addr, data );
}

int Kss_Core::cpu_in( time_t, addr_t addr )
{
	dprintf( "IN $%04X\n", addr );
	return 0xFF;
}

blargg_err_t Kss_Core::end_frame( time_t end )
{
	while ( cpu.time() < end )
	{
		time_t next = min( end, next_play );
		run_cpu( next );
		if ( cpu.r.pc == idle_addr )
			cpu.set_time( next );
		
		if ( cpu.time() >= next_play )
		{
			next_play += play_period;
			if ( cpu.r.pc == idle_addr )
			{
				if ( !gain_updated )
				{
					gain_updated = true;
					update_gain();
				}
				
				jsr( header_.play_addr );
			}
		}
	}
	
	next_play -= end;
	check( next_play >= 0 );
	cpu.adjust_time( -end );
	
	return blargg_ok;
}
