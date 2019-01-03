// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Nsf_Impl.h"

#include "blargg_endian.h"

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

// number of frames until play interrupts init
int const initial_play_delay = 7; // KikiKaikai needed this to work
int const bank_size = 0x1000;
int const rom_addr  = 0x8000;

int Nsf_Impl::read_code( addr_t addr ) const
{
	return *cpu.get_code( addr );
}

int Nsf_Impl::pcm_read( void* self, int addr )
{
	return STATIC_CAST(Nsf_Impl*,self)->read_code( addr );
}

Nsf_Impl::Nsf_Impl() : rom( bank_size ), enable_w4011( true )
{
	apu.dmc_reader( pcm_read, this );
	assert( offsetof (header_t,unused [4]) == header_t::size );
}

void Nsf_Impl::unload()
{
	rom.clear();
	high_ram.clear();
	Gme_Loader::unload();
}

Nsf_Impl::~Nsf_Impl() { unload(); }

bool nsf_header_t::valid_tag() const
{
	return 0 == memcmp( tag, "NESM\x1A", 5 );
}

double nsf_header_t::clock_rate() const
{
	return pal_only() ? 1662607.125 : 1789772.727272727;
}

int nsf_header_t::play_period() const
{
	// NTSC
	int         clocks   = 29780;
	int         value    = 0x411A;
	byte const* rate_ptr = ntsc_speed;
	
	// PAL
	if ( pal_only() )
	{
		clocks   = 33247;
		value    = 0x4E20;
		rate_ptr = pal_speed;
	}
	
	// Default rate
	int rate = get_le16( rate_ptr );
	if ( rate == 0 )
		rate = value;
	
	// Custom rate
	if ( rate != value )
		clocks = (int) (rate * clock_rate() * (1.0/1000000.0));
	
	return clocks;
}

// Gets address, given pointer to it in file header. If zero, returns rom_addr.
Nsf_Impl::addr_t Nsf_Impl::get_addr( byte const in [] )
{
	addr_t addr = get_le16( in );
	if ( addr == 0 )
		addr = rom_addr;
	return addr;
}

blargg_err_t Nsf_Impl::load_( Data_Reader& in )
{
	// pad ROM data with 0
	RETURN_ERR( rom.load( in, header_.size, &header_, 0 ) );
	
	if ( !header_.valid_tag() )
		return blargg_err_file_type;
	
	RETURN_ERR( high_ram.resize( (fds_enabled() ? fdsram_offset + fdsram_size : fdsram_offset) ) );

	addr_t load_addr = get_addr( header_.load_addr );
	if ( load_addr < (fds_enabled() ? sram_addr : rom_addr) )
		set_warning( "Load address is too low" );
	
	rom.set_addr( load_addr % bank_size );
	
	if ( header_.vers != 1 )
		set_warning( "Unknown file version" );
	
	set_play_period( header_.play_period() );
	
	return blargg_ok;
}

void Nsf_Impl::write_bank( int bank, int data )
{
	// Find bank in ROM
	int offset = rom.mask_addr( data * bank_size );
	if ( offset >= rom.size() )
		special_event( "invalid bank" );
	void const* rom_data = rom.at_addr( offset );
	
	#if !NSF_EMU_APU_ONLY
		if ( bank < bank_count - fds_banks && fds_enabled() )
		{
			// TODO: FDS bank switching is kind of hacky, might need to
			// treat ROM as RAM so changes won't get lost when switching.
			byte* out = sram();
			if ( bank >= fds_banks )
			{
				out = fdsram();
				bank -= fds_banks;
			}
			memcpy( &out [bank * bank_size], rom_data, bank_size );
			return;
		}
	#endif
	
	if ( bank >= fds_banks )
		cpu.map_code( (bank + 6) * bank_size, bank_size, rom_data );
}

void Nsf_Impl::map_memory()
{
	// Map standard things
	cpu.reset( unmapped_code() );
	cpu.map_code( 0, 0x2000, low_ram, low_ram_size ); // mirrored four times
	cpu.map_code( sram_addr, sram_size, sram() );
	
	// Determine initial banks
	byte banks [bank_count];
	static byte const zero_banks [sizeof header_.banks] = { 0 };
	if ( memcmp( header_.banks, zero_banks, sizeof zero_banks ) )
	{
		banks [0] = header_.banks [6];
		banks [1] = header_.banks [7];
		memcpy( banks + fds_banks, header_.banks, sizeof header_.banks );
	}
	else
	{
		// No initial banks, so assign them based on load_addr
		int first_bank = (get_addr( header_.load_addr ) - sram_addr) / bank_size;
		unsigned total_banks = rom.size() / bank_size;
		for ( int i = bank_count; --i >= 0; )
		{
			int bank = i - first_bank;
			if ( (unsigned) bank >= total_banks )
				bank = 0;
			banks [i] = bank;
		}
	}
	
	// Map banks
	for ( int i = (fds_enabled() ? 0 : fds_banks); i < bank_count; ++i )
		write_bank( i, banks [i] );
	
	// Map FDS RAM
	if ( fds_enabled() )
		cpu.map_code( rom_addr, fdsram_size, fdsram() );
}

inline void Nsf_Impl::push_byte( int b )
{
	low_ram [0x100 + cpu.r.sp--] = b;
}

// Jumps to routine, given pointer to address in file header. Pushes idle_addr
// as return address, NOT old PC.
void Nsf_Impl::jsr_then_stop( byte const addr [] )
{
	cpu.r.pc = get_addr( addr );
	push_byte( (idle_addr - 1) >> 8 );
	push_byte( (idle_addr - 1) );
}

blargg_err_t Nsf_Impl::start_track( int track )
{
	int speed_flags = 0;
	#if NSF_EMU_EXTRA_FLAGS
		speed_flags = header().speed_flags;
	#endif
	
	apu.reset( header().pal_only(), (speed_flags & 0x20) ? 0x3F : 0 );
	apu.enable_w4011_( enable_w4011 );
	apu.write_register( 0, 0x4015, 0x0F );
	apu.write_register( 0, 0x4017, (speed_flags & 0x10) ? 0x80 : 0 );
	
	// Clear memory
	memset( unmapped_code(), Nes_Cpu::halt_opcode, unmapped_size );
	memset( low_ram, 0, low_ram_size );
	memset( sram(), 0, sram_size );
	
	map_memory();
	
	// Arrange time of first call to play routine
	play_extra = 0;
	next_play  = play_period;
	
	play_delay = initial_play_delay;
	saved_state.pc = idle_addr;
	
	// Setup for call to init routine
	cpu.r.a  = track;
	cpu.r.x  = header_.pal_only();
	cpu.r.sp = 0xFF;
	jsr_then_stop( header_.init_addr );
	if ( cpu.r.pc < get_addr( header_.load_addr ) )
		set_warning( "Init address < load address" );
	
	return blargg_ok;
}

void Nsf_Impl::unmapped_write( addr_t addr, int data )
{
	dprintf( "Unmapped write $%04X <- %02X\n", (int) addr, data );
}

int Nsf_Impl::unmapped_read( addr_t addr )
{
	dprintf( "Unmapped read $%04X\n", (int) addr );
	return addr >> 8;
}

void Nsf_Impl::special_event( const char str [] )
{
	dprintf( "%s\n", str );
}

void Nsf_Impl::run_once( time_t end )
{
	// Emulate until next play call if possible
	if ( run_cpu_until( min( next_play, end ) ) )
	{
		// Halt instruction encountered
		
		if ( cpu.r.pc != idle_addr )
		{
			special_event( "illegal instruction" );
			cpu.count_error();
			cpu.set_time( cpu.end_time() );
			return;
		}

		// Init/play routine returned
		play_delay = 1; // play can now be called regularly
		
		if ( saved_state.pc == idle_addr )
		{
			// nothing to run
			time_t t = cpu.end_time();
			if ( cpu.time() < t )
				cpu.set_time( t );
		}
		else
		{
			// continue init routine that was interrupted by play routine
			cpu.r = saved_state;
			saved_state.pc = idle_addr;
		}
	}
	
	if ( time() >= next_play )
	{
		// Calculate time of next call to play routine
		play_extra ^= 1; // extra clock every other call
		next_play += play_period + play_extra;
		
		// Call routine if ready
		if ( play_delay && !--play_delay )
		{
			// Save state if init routine is still running
			if ( cpu.r.pc != idle_addr )
			{
				check( saved_state.pc == idle_addr );
				saved_state = cpu.r;
				special_event( "play called during init" );
			}
			
			jsr_then_stop( header_.play_addr );
		}
	}
}

void Nsf_Impl::run_until( time_t end )
{
	while ( time() < end )
		run_once( end );
}

void Nsf_Impl::end_frame( time_t end )
{
	if ( time() < end )
		run_until( end );
	cpu.adjust_time( -end );
	
	// Localize to new time frame
	next_play -= end;
	check( next_play >= 0 );
	if ( next_play < 0 )
		next_play = 0;
	
	apu.end_frame( end );
}
