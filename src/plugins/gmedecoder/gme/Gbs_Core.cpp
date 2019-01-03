// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gbs_Core.h"

#include "blargg_endian.h"

/* Copyright (C) 2003-2009 Shay Green. This module is free software; you
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

int const tempo_unit = 16;
int const idle_addr = 0xF00D;
int const bank_size = 0x4000;

Gbs_Core::Gbs_Core() : rom( bank_size )
{
	tempo = tempo_unit;
	assert( offsetof (header_t,copyright [32]) == header_t::size );
}

Gbs_Core::~Gbs_Core() { }

void Gbs_Core::unload()
{
	header_.timer_mode = 0; // set_tempo() reads this
	rom.clear();
	Gme_Loader::unload();
}

bool Gbs_Core::header_t::valid_tag() const
{
	return 0 == memcmp( tag, "GBS", 3 );
}

blargg_err_t Gbs_Core::load_( Data_Reader& in )
{
	RETURN_ERR( rom.load( in, header_.size, &header_, 0 ) );
	
	if ( !header_.valid_tag() )
		return blargg_err_file_type;
	
	if ( header_.vers < 1 || header_.vers > 2 )
		set_warning( "Unknown file version" );
	
	if ( header_.timer_mode & 0x78 )
		set_warning( "Invalid timer mode" );
	
	addr_t load_addr = get_le16( header_.load_addr );
	if ( (header_.load_addr [1] | header_.init_addr [1] | header_.play_addr [1]) > 0x7F ||
			load_addr < 0x400 )
		set_warning( "Invalid load/init/play address" );
	
	cpu.rst_base = load_addr;
	rom.set_addr( load_addr );
	
	return blargg_ok;
}

void Gbs_Core::set_bank( int n )
{
	addr_t addr = rom.mask_addr( n * bank_size );
	if ( addr == 0 && rom.size() > bank_size )
		addr = bank_size; // MBC1&2 behavior, bank 0 acts like bank 1
	cpu.map_code( bank_size, bank_size, rom.at_addr( addr ) );
}

void Gbs_Core::update_timer()
{
	play_period_ = 70224 / tempo_unit; // 59.73 Hz
	
	if ( header_.timer_mode & 0x04 ) 
	{
		// Using custom rate
		static byte const rates [4] = { 6, 0, 2, 4 };
		// TODO: emulate double speed CPU mode rather than halving timer rate
		int double_speed = header_.timer_mode >> 7;
		int shift = rates [ram [hi_page + 7] & 3] - double_speed;
		play_period_ = (256 - ram [hi_page + 6]) << shift;
	}
	
	play_period_ *= tempo;
}

void Gbs_Core::set_tempo( double t )
{
	tempo = (int) (tempo_unit / t + 0.5);
	apu_.set_tempo( t );
	update_timer();
}

// Jumps to routine, given pointer to address in file header. Pushes idle_addr
// as return address, NOT old PC.
void Gbs_Core::jsr_then_stop( byte const addr [] )
{
	check( cpu.r.sp == get_le16( header_.stack_ptr ) );
	cpu.r.pc = get_le16( addr );
	write_mem( --cpu.r.sp, idle_addr >> 8 );
	write_mem( --cpu.r.sp, idle_addr      );
}

blargg_err_t Gbs_Core::start_track( int track, Gb_Apu::mode_t mode )
{
	// Reset APU to state expected by most rips
	static byte const sound_data [] = {
		0x80, 0xBF, 0x00, 0x00, 0xB8, // square 1 DAC disabled
		0x00, 0x3F, 0x00, 0x00, 0xB8, // square 2 DAC disabled
		0x7F, 0xFF, 0x9F, 0x00, 0xB8, // wave     DAC disabled
		0x00, 0xFF, 0x00, 0x00, 0xB8, // noise    DAC disabled
		0x77, 0xFF, 0x80, // max volume, all chans in center, power on
	};
	apu_.reset( mode );
	apu_.write_register( 0, 0xFF26, 0x80 ); // power on
	for ( int i = 0; i < (int) sizeof sound_data; i++ )
		apu_.write_register( 0, i + apu_.io_addr, sound_data [i] );
	apu_.end_frame( 1 ); // necessary to get click out of the way
	
	// Init memory and I/O registers
	memset( ram, 0, 0x4000 );
	memset( ram + 0x4000, 0xFF, 0x1F80 );
	memset( ram + 0x5F80, 0, sizeof ram - 0x5F80 );
	ram [hi_page] = 0; // joypad reads back as 0
	ram [idle_addr - ram_addr] = 0xED; // illegal instruction
	ram [hi_page + 6] = header_.timer_modulo;
	ram [hi_page + 7] = header_.timer_mode;
	
	// Map memory
	cpu.reset( rom.unmapped() );
	cpu.map_code( ram_addr, 0x10000 - ram_addr, ram );
	cpu.map_code( 0, bank_size, rom.at_addr( 0 ) );
	set_bank( rom.size() > bank_size );
	
	// CPU registers, timing
	update_timer();
	next_play = play_period_;
	cpu.r.fa = track;
	cpu.r.sp = get_le16( header_.stack_ptr );
	jsr_then_stop( header_.init_addr );
	
	return blargg_ok;
}

blargg_err_t Gbs_Core::run_until( int end )
{
	end_time = end;
	cpu.set_time( cpu.time() - end );
	while ( true )
	{
		run_cpu();
		if ( cpu.time() >= 0 )
			break;
		
		if ( cpu.r.pc == idle_addr )
		{
			if ( next_play > end_time )
			{
				cpu.set_time( 0 );
				break;
			}
			
			if ( cpu.time() < next_play - end_time )
				cpu.set_time( next_play - end_time );
			next_play += play_period_;
			jsr_then_stop( header_.play_addr );
		}
		else if ( cpu.r.pc > 0xFFFF )
		{
			dprintf( "PC wrapped around\n" );
			cpu.r.pc &= 0xFFFF;
		}
		else
		{
			set_warning( "Emulation error (illegal/unsupported instruction)" );
			dprintf( "Bad opcode $%02X at $%04X\n",
					(int) *cpu.get_code( cpu.r.pc ), (int) cpu.r.pc );
			cpu.r.pc = (cpu.r.pc + 1) & 0xFFFF;
			cpu.set_time( cpu.time() + 6 );
		}
	}
	
	return blargg_ok;
}

blargg_err_t Gbs_Core::end_frame( int end )
{
	RETURN_ERR( run_until( end ) );
	
	next_play -= end;
	if ( next_play < 0 ) // happens when play routine takes too long
	{
		#if !GBS_IGNORE_STARVED_PLAY
			check( false );
		#endif
		next_play = 0;
	}
	
	apu_.end_frame( end );
	
	return blargg_ok;
}
