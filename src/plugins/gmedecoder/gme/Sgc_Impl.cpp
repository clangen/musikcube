// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sgc_Impl.h"

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

void const* Sgc_Impl::coleco_bios;

Sgc_Impl::Sgc_Impl() :
	rom( bank_size )
{
	assert( offsetof (header_t,copyright [32]) == header_t::size );
}

Sgc_Impl::~Sgc_Impl()
{ }

bool Sgc_Impl::header_t::valid_tag() const
{
	return 0 == memcmp( tag, "SGC\x1A", 4 );
}

blargg_err_t Sgc_Impl::load_( Data_Reader& in )
{
	RETURN_ERR( rom.load( in, header_.size, &header_, 0 ) );
	
	if ( !header_.valid_tag() )
		return blargg_err_file_type;
	
	if ( header_.vers != 1 )
		set_warning( "Unknown file version" );
	
	if ( header_.system > 2 )
		set_warning( "Unknown system" );
	
	addr_t load_addr = get_le16( header_.load_addr );
	if ( load_addr < 0x400 )
		set_warning( "Invalid load address" );
	
	rom.set_addr( load_addr );
	play_period = clock_rate() / 60;
	
	if ( sega_mapping() )
	{
		RETURN_ERR( ram.resize( 0x2000 + Sgc_Cpu::page_padding ) );
		RETURN_ERR( ram2.resize( bank_size + Sgc_Cpu::page_padding ) );
	}
	else
	{
		RETURN_ERR( ram.resize( 0x400 + Sgc_Cpu::page_padding ) );
	}
	
	RETURN_ERR( vectors.resize( Sgc_Cpu::page_size + Sgc_Cpu::page_padding ) );
	
	// TODO: doesn't need to be larger than page size, if we do mapping calls right
	RETURN_ERR( unmapped_write.resize( bank_size ) );
	
	return blargg_ok;
}

void Sgc_Impl::unload()
{
	rom.clear();
	vectors.clear();
	ram.clear();
	ram2.clear();
	unmapped_write.clear();
	Gme_Loader::unload();
}

blargg_err_t Sgc_Impl::start_track( int track )
{
	memset( ram .begin(), 0, ram .size() );
	memset( ram2.begin(), 0, ram2.size() );
	memset( vectors.begin(), 0xFF, vectors.size() );
	cpu.reset( unmapped_write.begin(), rom.unmapped() );
	
	if ( sega_mapping() )
	{
		vectors_addr = 0x10000 - Sgc_Cpu::page_size;
		idle_addr = vectors_addr;
		for ( int i = 1; i < 8; ++i )
		{
			vectors [i*8 + 0] = 0xC3; // JP addr
			vectors [i*8 + 1] = header_.rst_addrs [i*2 + 0];
			vectors [i*8 + 2] = header_.rst_addrs [i*2 + 1];
		}
		
		cpu.map_mem( 0xC000, 0x2000, ram.begin() );
		cpu.map_mem( vectors_addr, cpu.page_size, unmapped_write.begin(), vectors.begin() );
		
		bank2 = NULL;
		for ( int i = 0; i < 4; ++i )
			cpu_write( 0xFFFC + i, header_.mapping [i] );
	}
	else
	{
		if ( !coleco_bios )
			return BLARGG_ERR( BLARGG_ERR_CALLER, "Coleco BIOS not set" );
		
		vectors_addr = 0;
		cpu.map_mem( 0, 0x2000, unmapped_write.begin(), coleco_bios );
		for ( int i = 0; i < 8; ++i )
			cpu.map_mem( 0x6000 + i*0x400, 0x400, ram.begin() );
		
		idle_addr = 0x2000;
		cpu.map_mem( 0x2000, cpu.page_size, unmapped_write.begin(), vectors.begin() );
		
		for ( int i = 0; i < 0x8000 / bank_size; ++i )
		{
			int addr = 0x8000 + i*bank_size;
			cpu.map_mem( addr, bank_size, unmapped_write.begin(), rom.at_addr( addr ) );
		}
	}
	
	cpu.r.sp  = get_le16( header_.stack_ptr );
	cpu.r.b.a = track;
	next_play = play_period;
	
	jsr( header_.init_addr );
	
	return blargg_ok;
}

// Emulation

void Sgc_Impl::jsr( byte const (&addr) [2] )
{
	*cpu.write( --cpu.r.sp ) = idle_addr >> 8;
	*cpu.write( --cpu.r.sp ) = idle_addr & 0xFF;
	cpu.r.pc = get_le16( addr );
}

void Sgc_Impl::set_bank( int bank, void const* data )
{
	//dprintf( "map bank %d to %p\n", bank, (byte*) data - rom.at_addr( 0 ) );
	cpu.map_mem( bank * bank_size, bank_size, unmapped_write.begin(), data );
}

void Sgc_Impl::cpu_write( addr_t addr, int data )
{
	if ( (addr ^ 0xFFFC) > 3 || !sega_mapping() )
	{
		*cpu.write( addr ) = data;
		return;
	}
	
	switch ( addr )
	{
	case 0xFFFC:
		cpu.map_mem( 2 * bank_size, bank_size, ram2.begin() );
		if ( data & 0x08 )
			break;
		
		bank2 = ram2.begin();
		// FALL THROUGH
	
	case 0xFFFF: {
		bool rom_mapped = (cpu.read( 2 * bank_size ) == bank2);
		bank2 = rom.at_addr( data * bank_size );
		if ( rom_mapped )
			set_bank( 2, bank2 );
		break;
	}
		
	case 0xFFFD:
		set_bank( 0, rom.at_addr( data * bank_size ) );
		break;
	
	case 0xFFFE:
		set_bank( 1, rom.at_addr( data * bank_size ) );
		break;
	}
}

int Sgc_Impl::cpu_in( addr_t addr )
{
	dprintf( "in %02X\n", addr );
	return 0;
}
	
void Sgc_Impl::cpu_out( time_t, addr_t addr, int )
{
	dprintf( "out %02X\n", addr & 0xFF );
}

blargg_err_t Sgc_Impl::end_frame( time_t end )
{
	while ( cpu.time() < end )
	{
		time_t next = min( end, next_play );
		if ( run_cpu( next ) )
		{
			set_warning( "Unsupported CPU instruction" );
			cpu.set_time( next );
		}
		
		if ( cpu.r.pc == idle_addr )
			cpu.set_time( next );
		
		if ( cpu.time() >= next_play )
		{
			next_play += play_period;
			if ( cpu.r.pc == idle_addr )
				jsr( header_.play_addr );
		}
	}
	
	next_play -= end;
	check( next_play >= 0 );
	cpu.adjust_time( -end );
	
	return blargg_ok;
}
