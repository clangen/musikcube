// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Ay_Emu.h"

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

// TODO: probably don't need detailed errors as to why file is corrupt

int const spectrum_clock = 3546900; // 128K Spectrum
int const spectrum_period = 70908;

//int const spectrum_clock = 3500000; // 48K Spectrum
//int const spectrum_period = 69888;

int const cpc_clock = 2000000;

Ay_Emu::Ay_Emu()
{
	core.set_cpc_callback( enable_cpc_, this );
	set_type( gme_ay_type );
	set_silence_lookahead( 6 );
}

Ay_Emu::~Ay_Emu() { }

// Track info

// Given pointer to 2-byte offset of data, returns pointer to data, or NULL if
// offset is 0 or there is less than min_size bytes of data available.
static byte const* get_data( Ay_Emu::file_t const& file, byte const ptr [], int min_size )
{
	int offset = (BOOST::int16_t) get_be16( ptr );
	int pos  = ptr - (byte const*) file.header;
	int size = file.end - (byte const*) file.header;
	assert( (unsigned) pos <= (unsigned) size - 2 );
	int limit = size - min_size;
	if ( limit < 0 || !offset || (unsigned) (pos + offset) > (unsigned) limit )
		return NULL;
	return ptr + offset;
}

static blargg_err_t parse_header( byte const in [], int size, Ay_Emu::file_t* out )
{
	typedef Ay_Emu::header_t header_t;
	if ( size < header_t::size )
		return blargg_err_file_type;
	
	out->header = (header_t const*) in;
	out->end    = in + size;
	header_t const& h = *(header_t const*) in;
	if ( memcmp( h.tag, "ZXAYEMUL", 8 ) )
		return blargg_err_file_type;
	
	out->tracks = get_data( *out, h.track_info, (h.max_track + 1) * 4 );
	if ( !out->tracks )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "missing track data" );
	
	return blargg_ok;
}

static void copy_ay_fields( Ay_Emu::file_t const& file, track_info_t* out, int track )
{
	Gme_File::copy_field_( out->song, (char const*) get_data( file, file.tracks + track * 4, 1 ) );
	byte const* track_info = get_data( file, file.tracks + track * 4 + 2, 6 );
	if ( track_info )
		out->length = get_be16( track_info + 4 ) * (1000 / 50); // frames to msec
	
	Gme_File::copy_field_( out->author,  (char const*) get_data( file, file.header->author, 1 ) );
	Gme_File::copy_field_( out->comment, (char const*) get_data( file, file.header->comment, 1 ) );
}

static void hash_ay_file( Ay_Emu::file_t const& file, Gme_Info_::Hash_Function& out )
{
	out.hash_( &file.header->vers, sizeof(file.header->vers) );
	out.hash_( &file.header->player, sizeof(file.header->player) );
	out.hash_( &file.header->unused[0], sizeof(file.header->unused) );
	out.hash_( &file.header->max_track, sizeof(file.header->max_track) );
	out.hash_( &file.header->first_track, sizeof(file.header->first_track) );

	for ( unsigned i = 0; i <= file.header->max_track; i++ )
	{
		byte const* track_info = get_data( file, file.tracks + i * 4 + 2, 14 );
		if ( track_info )
		{
			out.hash_( track_info + 8, 2 );
			byte const* points = get_data( file, track_info + 10, 6 );
			if ( points ) out.hash_( points, 6 );

			byte const* blocks = get_data( file, track_info + 12, 8 );
			if ( blocks )
			{
				int addr = get_be16( blocks );

				while ( addr )
				{
					out.hash_( blocks, 4 );

					int len = get_be16( blocks + 2 );

					byte const* block = get_data( file, blocks + 4, len );
					if ( block ) out.hash_( block, len );

					blocks += 6;
					addr = get_be16( blocks );
				}
			}
		}
	}
}

blargg_err_t Ay_Emu::track_info_( track_info_t* out, int track ) const
{
	copy_ay_fields( file, out, track );
	return blargg_ok;
}

struct Ay_File : Gme_Info_
{
	Ay_Emu::file_t file;
	
	Ay_File() { set_type( gme_ay_type ); }
	
	blargg_err_t load_mem_( byte const begin [], int size )
	{
		RETURN_ERR( parse_header( begin, size, &file ) );
		set_track_count( file.header->max_track + 1 );
		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int track ) const
	{
		copy_ay_fields( file, out, track );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_ay_file( file, out );
		return blargg_ok;
	}
};

static Music_Emu* new_ay_emu ()
{
	return BLARGG_NEW Ay_Emu;
}

static Music_Emu* new_ay_file()
{
	return BLARGG_NEW Ay_File;
}

gme_type_t_ const gme_ay_type [1] = {{
	"ZX Spectrum",
	0,
	&new_ay_emu,
	&new_ay_file,
	"AY",
	1
}};

// Setup

blargg_err_t Ay_Emu::load_mem_( byte const in [], int size )
{
	assert( offsetof (header_t,track_info [2]) == header_t::size );
	
	RETURN_ERR( parse_header( in, size, &file ) );
	set_track_count( file.header->max_track + 1 );
	
	if ( file.header->vers > 2 )
		set_warning( "Unknown file version" );
	
	int const osc_count = Ay_Apu::osc_count + 1; // +1 for beeper
	
	set_voice_count( osc_count );
	core.apu().volume( gain() );
	
	static const char* const names [osc_count] = {
		"Wave 1", "Wave 2", "Wave 3", "Beeper"
	};
	set_voice_names( names );
	
	static int const types [osc_count] = {
		wave_type+0, wave_type+1, wave_type+2, mixed_type+1
	};
	set_voice_types( types );
	
	return setup_buffer( spectrum_clock );
}
	
void Ay_Emu::update_eq( blip_eq_t const& eq )
{
	core.apu().treble_eq( eq );
}

void Ay_Emu::set_voice( int i, Blip_Buffer* center, Blip_Buffer*, Blip_Buffer* )
{
	if ( i >= Ay_Apu::osc_count )
		core.set_beeper_output( center );
	else
		core.apu().set_output( i, center );
}

void Ay_Emu::set_tempo_( double t )
{
	int p = spectrum_period;
	if ( clock_rate() != spectrum_clock )
		p = clock_rate() / 50;
	
	core.set_play_period( blip_time_t (p / t) );
}

blargg_err_t Ay_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	byte* const mem = core.mem();
	
	memset( mem + 0x0000, 0xC9, 0x100 ); // fill RST vectors with RET
	memset( mem + 0x0100, 0xFF, 0x4000 - 0x100 );
	memset( mem + core.ram_addr, 0x00, core.mem_size - core.ram_addr );

	// locate data blocks
	byte const* const data = get_data( file, file.tracks + track * 4 + 2, 14 );
	if ( !data )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "file data missing" );
	
	byte const* const more_data = get_data( file, data + 10, 6 );
	if ( !more_data )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "file data missing" );
	
	byte const* blocks = get_data( file, data + 12, 8 );
	if ( !blocks )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "file data missing" );
	
	// initial addresses
	unsigned addr = get_be16( blocks );
	if ( !addr )
		return BLARGG_ERR( BLARGG_ERR_FILE_CORRUPT, "file data missing" );
	
	unsigned init = get_be16( more_data + 2 );
	if ( !init )
		init = addr;
	
	// copy blocks into memory
	do
	{
		blocks += 2;
		unsigned len = get_be16( blocks ); blocks += 2;
		if ( addr + len > core.mem_size )
		{
			set_warning( "Bad data block size" );
			len = core.mem_size - addr;
		}
		check( len );
		byte const* in = get_data( file, blocks, 0 ); blocks += 2;
		if ( len > (unsigned) (file.end - in) )
		{
			set_warning( "File data missing" );
			len = file.end - in;
		}
		//dprintf( "addr: $%04X, len: $%04X\n", addr, len );
		if ( addr < core.ram_addr && addr >= 0x400 ) // several tracks use low data
			dprintf( "Block addr in ROM\n" );
		memcpy( mem + addr, in, len );
		
		if ( file.end - blocks < 8 )
		{
			set_warning( "File data missing" );
			break;
		}
	}
	while ( (addr = get_be16( blocks )) != 0 );
	
	// copy and configure driver
	static byte const passive [] = {
		0xF3,       // DI
		0xCD, 0, 0, // CALL init
		0xED, 0x5E, // LOOP: IM 2
		0xFB,       // EI
		0x76,       // HALT
		0x18, 0xFA  // JR LOOP
	};
	static byte const active [] = {
		0xF3,       // DI
		0xCD, 0, 0, // CALL init
		0xED, 0x56, // LOOP: IM 1
		0xFB,       // EI
		0x76,       // HALT
		0xCD, 0, 0, // CALL play
		0x18, 0xF7  // JR LOOP
	};
	memcpy( mem, passive, sizeof passive );
	int const play_addr = get_be16( more_data + 4 );
	if ( play_addr )
	{
		memcpy( mem, active, sizeof active );
		mem [ 9] = play_addr;
		mem [10] = play_addr >> 8;
	}
	mem [2] = init;
	mem [3] = init >> 8;
	
	mem [0x38] = 0xFB; // Put EI at interrupt vector (followed by RET)
	
	// start at spectrum speed
	change_clock_rate( spectrum_clock );
	set_tempo( tempo() );
	
	Ay_Core::registers_t r = { };
	r.sp = get_be16( more_data );
	r.b.a     = r.b.b = r.b.d = r.b.h = data [8];
	r.b.flags = r.b.c = r.b.e = r.b.l = data [9];
	r.alt.w = r.w;
	r.ix = r.iy = r.w.hl;
	
	core.start_track( r, play_addr );
	
	return blargg_ok;
}

blargg_err_t Ay_Emu::run_clocks( blip_time_t& duration, int )
{
	core.end_frame( &duration );
	return blargg_ok;
}

inline void Ay_Emu::enable_cpc()
{
	change_clock_rate( cpc_clock );
	set_tempo( tempo() );
}

void Ay_Emu::enable_cpc_( void* data )
{
	STATIC_CAST(Ay_Emu*,data)->enable_cpc();
}

blargg_err_t Ay_Emu::hash_( Hash_Function& out ) const
{
	hash_ay_file( file, out );
	return blargg_ok;
}
