// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Hes_Emu.h"

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

Hes_Emu::Hes_Emu()
{
	set_type( gme_hes_type );
	set_silence_lookahead( 6 );
	set_gain( 1.11 );
}

Hes_Emu::~Hes_Emu() { }

void Hes_Emu::unload()
{
	core.unload();
	Music_Emu::unload();
}

static byte const* copy_field( byte const in [], char* out )
{
	if ( in )
	{
		int len = 0x20;
		if ( in [0x1F] && !in [0x2F] )
			len = 0x30; // fields are sometimes 16 bytes longer (ugh)
		
		// since text fields are where any data could be, detect non-text
		// and fields with data after zero byte terminator
		
		int i = 0;
		for ( ; i < len && in [i]; i++ )
			if ( (unsigned) (in [i] - ' ') >= 0xFF - ' ' ) // also treat 0xFF as non-text
				return 0; // non-ASCII found
		
		for ( ; i < len; i++ )
			if ( in [i] )
				return 0; // data after terminator
		
		Gme_File::copy_field_( out, (char const*) in, len );
		in += len;
	}
	return in;
}

static byte const* copy_hes_fields( byte const in [], track_info_t* out )
{
	byte const* in_offset = in;
	if ( *in_offset >= ' ' )
	{
		in_offset = copy_field( in_offset, out->game      );
		in_offset = copy_field( in_offset, out->author    );
		in_offset = copy_field( in_offset, out->copyright );
	}
	return in_offset ? in_offset : in;
}

static void hash_hes_file( Hes_Emu::header_t const& h, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.vers, sizeof(h.vers) );
	out.hash_( &h.first_track, sizeof(h.first_track) );
	out.hash_( &h.init_addr[0], sizeof(h.init_addr) );
	out.hash_( &h.banks[0], sizeof(h.banks) );
	out.hash_( &h.data_size[0], sizeof(h.data_size) );
	out.hash_( &h.addr[0], sizeof(h.addr) );
	out.hash_( &h.unused[0], sizeof(h.unused) );
	out.hash_( data, Hes_Core::info_offset );

    track_info_t temp; // GCC whines about passing a pointer to a temporary here
    byte const* more_data = copy_hes_fields( data + Hes_Core::info_offset, &temp );
	out.hash_( more_data, data_size - ( more_data - data ) );
}

blargg_err_t Hes_Emu::track_info_( track_info_t* out, int ) const
{
	copy_hes_fields( core.data() + core.info_offset, out );
	return blargg_ok;
}

struct Hes_File : Gme_Info_
{
	enum { fields_offset = Hes_Core::header_t::size + Hes_Core::info_offset };
	
	union header_t {
		Hes_Core::header_t header;
		byte data [fields_offset + 0x30 * 3];
	} const* h;
	
	Hes_File()
	{
		set_type( gme_hes_type );
	}
	
	blargg_err_t load_mem_( byte const begin [], int size )
	{
		h = ( header_t const* ) begin;
		
		if ( !h->header.valid_tag() )
			return blargg_err_file_type;
		
		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_hes_fields( h->data + fields_offset, out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_hes_file( h->header, file_begin() + h->header.size, file_end() - file_begin() - h->header.size, out );
		return blargg_ok;
	}
};

static Music_Emu* new_hes_emu () { return BLARGG_NEW Hes_Emu ; }
static Music_Emu* new_hes_file() { return BLARGG_NEW Hes_File; }

gme_type_t_ const gme_hes_type [1] = {{ "PC Engine", 256, &new_hes_emu, &new_hes_file, "HES", 1 }};

blargg_err_t Hes_Emu::load_( Data_Reader& in )
{
	RETURN_ERR( core.load( in ) );
	
	static const char* const names [Hes_Apu::osc_count + Hes_Apu_Adpcm::osc_count] = {
		"Wave 1", "Wave 2", "Wave 3", "Wave 4", "Multi 1", "Multi 2", "ADPCM"
	};
	set_voice_names( names );
	
	static int const types [Hes_Apu::osc_count + Hes_Apu_Adpcm::osc_count] = {
		wave_type+0, wave_type+1, wave_type+2, wave_type+3, mixed_type+0, mixed_type+1, mixed_type+2
	};
	set_voice_types( types );
	
	set_voice_count( core.apu().osc_count + core.adpcm().osc_count );
	core.apu().volume( gain() );
	core.adpcm().volume( gain() );
	
    return setup_buffer( 7159091 );
}

void Hes_Emu::update_eq( blip_eq_t const& eq )
{
	core.apu().treble_eq( eq );
    core.adpcm().treble_eq( eq );
}

void Hes_Emu::set_voice( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	if ( i < core.apu().osc_count )
		core.apu().set_output( i, c, l, r );
	else if ( i == core.apu().osc_count )
		core.adpcm().set_output( 0, c, l, r );
}

void Hes_Emu::set_tempo_( double t )
{
	core.set_tempo( t );
}

blargg_err_t Hes_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	return core.start_track( track );
}

blargg_err_t Hes_Emu::run_clocks( blip_time_t& duration_, int )
{
	return core.end_frame( duration_ );
}

blargg_err_t Hes_Emu::hash_( Hash_Function& out ) const
{
	hash_hes_file( header(), core.data(), core.data_size(), out );
	return blargg_ok;
}
