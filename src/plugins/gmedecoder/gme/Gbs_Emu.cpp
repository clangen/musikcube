// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Gbs_Emu.h"

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

Gbs_Emu::equalizer_t const Gbs_Emu::handheld_eq   = { -47.0, 2000, 0,0,0,0,0,0,0,0 };
Gbs_Emu::equalizer_t const Gbs_Emu::cgb_eq        = {   0.0,  300, 0,0,0,0,0,0,0,0 };
Gbs_Emu::equalizer_t const Gbs_Emu::headphones_eq = {   0.0,   30, 0,0,0,0,0,0,0,0 }; // DMG

Gbs_Emu::Gbs_Emu()
{
	sound_hardware = sound_gbs;
	enable_clicking( false );
	set_type( gme_gbs_type );
	set_silence_lookahead( 6 );
	set_max_initial_silence( 21 );
	set_gain( 1.2 );

	// kind of midway between headphones and speaker
	static equalizer_t const eq = { -1.0, 120, 0,0,0,0,0,0,0,0 };
	set_equalizer( eq );
}

Gbs_Emu::~Gbs_Emu() { }

void Gbs_Emu::unload()
{
	core_.unload();
	Music_Emu::unload();
}

// Track info

static void copy_gbs_fields( Gbs_Emu::header_t const& h, track_info_t* out )
{
	GME_COPY_FIELD( h, out, game );
	GME_COPY_FIELD( h, out, author );
	GME_COPY_FIELD( h, out, copyright );
}

static void hash_gbs_file( Gbs_Emu::header_t const& h, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.vers, sizeof(h.vers) );
	out.hash_( &h.track_count, sizeof(h.track_count) );
	out.hash_( &h.first_track, sizeof(h.first_track) );
	out.hash_( &h.load_addr[0], sizeof(h.load_addr) );
	out.hash_( &h.init_addr[0], sizeof(h.init_addr) );
	out.hash_( &h.play_addr[0], sizeof(h.play_addr) );
	out.hash_( &h.stack_ptr[0], sizeof(h.stack_ptr) );
	out.hash_( &h.timer_modulo, sizeof(h.timer_modulo) );
	out.hash_( &h.timer_mode, sizeof(h.timer_mode) );
	out.hash_( data, data_size );
}

blargg_err_t Gbs_Emu::track_info_( track_info_t* out, int ) const
{
	copy_gbs_fields( header(), out );
	return blargg_ok;
}

struct Gbs_File : Gme_Info_
{
	Gbs_Emu::header_t const* h;
	
	Gbs_File() { set_type( gme_gbs_type ); }
	
	blargg_err_t load_mem_( byte const begin [], int size )
	{
		h = ( Gbs_Emu::header_t * ) begin;

		set_track_count( h->track_count );
		if ( !h->valid_tag() )
			return blargg_err_file_type;
		
		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_gbs_fields( Gbs_Emu::header_t( *h ), out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_gbs_file( *h, file_begin() + h->size, file_end() - file_begin() - h->size, out );
		return blargg_ok;
	}
};

static Music_Emu* new_gbs_emu () { return BLARGG_NEW Gbs_Emu ; }
static Music_Emu* new_gbs_file() { return BLARGG_NEW Gbs_File; }

gme_type_t_ const gme_gbs_type [1] = {{ "Game Boy", 0, &new_gbs_emu, &new_gbs_file, "GBS", 1 }};

// Setup

blargg_err_t Gbs_Emu::load_( Data_Reader& in )
{
	RETURN_ERR( core_.load( in ) );
	set_warning( core_.warning() );
	set_track_count( header().track_count );
	set_voice_count( Gb_Apu::osc_count );
	core_.apu().volume( gain() );
	
	static const char* const names [Gb_Apu::osc_count] = {
		"Square 1", "Square 2", "Wave", "Noise"
	};
	set_voice_names( names );
	
	static int const types [Gb_Apu::osc_count] = {
		wave_type+1, wave_type+2, wave_type+3, mixed_type+1
	};
	set_voice_types( types );
	
	return setup_buffer( 4194304 );
}

void Gbs_Emu::update_eq( blip_eq_t const& eq )
{
	core_.apu().treble_eq( eq );
}

void Gbs_Emu::set_voice( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	core_.apu().set_output( i, c, l, r );
}

void Gbs_Emu::set_tempo_( double t )
{
	core_.set_tempo( t );
}

blargg_err_t Gbs_Emu::start_track_( int track )
{
	sound_t mode = sound_hardware;
	if ( mode == sound_gbs )
		mode = (header().timer_mode & 0x80) ? sound_cgb : sound_dmg;
	
	RETURN_ERR( core_.start_track( track, (Gb_Apu::mode_t) mode ) );
	
	// clear buffer AFTER track is started, eliminating initial click
	return Classic_Emu::start_track_( track );
}

blargg_err_t Gbs_Emu::run_clocks( blip_time_t& duration, int )
{
	return core_.end_frame( duration );
}

blargg_err_t Gbs_Emu::hash_( Hash_Function& out ) const
{
	hash_gbs_file( header(), core_.rom_().begin(), core_.rom_().file_size(), out );
	return blargg_ok;
}