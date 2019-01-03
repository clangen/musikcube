// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Nsf_Emu.h"

#if !NSF_EMU_APU_ONLY
	#include "Nes_Namco_Apu.h"
	#include "Nes_Vrc6_Apu.h"
	#include "Nes_Fme7_Apu.h"
	#include "Nes_Fds_Apu.h"
	#include "Nes_Mmc5_Apu.h"
	#include "Nes_Vrc7_Apu.h"
#endif

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

Nsf_Emu::equalizer_t const Nsf_Emu::nes_eq     = {  -1.0, 80, 0,0,0,0,0,0,0,0 };
Nsf_Emu::equalizer_t const Nsf_Emu::famicom_eq = { -15.0, 80, 0,0,0,0,0,0,0,0 };

Nsf_Emu::Nsf_Emu()
{
	set_type( gme_nsf_type );
	set_silence_lookahead( 6 );
	set_gain( 1.4 );
	set_equalizer( nes_eq );
}

Nsf_Emu::~Nsf_Emu()
{
	unload();
}

void Nsf_Emu::unload()
{
	core_.unload();
	Music_Emu::unload();
}

// Track info

static void copy_nsf_fields( Nsf_Emu::header_t const& h, track_info_t* out )
{
	GME_COPY_FIELD( h, out, game );
	GME_COPY_FIELD( h, out, author );
	GME_COPY_FIELD( h, out, copyright );
	if ( h.chip_flags )
		Music_Emu::copy_field_( out->system, "Famicom" );
}

void hash_nsf_file( Nsf_Core::header_t const& h, unsigned char const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.vers, sizeof(h.vers) );
	out.hash_( &h.track_count, sizeof(h.track_count) );
	out.hash_( &h.first_track, sizeof(h.first_track) );
	out.hash_( &h.load_addr[0], sizeof(h.load_addr) );
	out.hash_( &h.init_addr[0], sizeof(h.init_addr) );
	out.hash_( &h.play_addr[0], sizeof(h.play_addr) );
	out.hash_( &h.ntsc_speed[0], sizeof(h.ntsc_speed) );
	out.hash_( &h.banks[0], sizeof(h.banks) );
	out.hash_( &h.pal_speed[0], sizeof(h.pal_speed) );
	out.hash_( &h.speed_flags, sizeof(h.speed_flags) );
	out.hash_( &h.chip_flags, sizeof(h.chip_flags) );
	out.hash_( &h.unused[0], sizeof(h.unused) );

	out.hash_( data, data_size );
}

blargg_err_t Nsf_Emu::track_info_( track_info_t* out, int ) const
{
	copy_nsf_fields( header(), out );
	return blargg_ok;
}

static blargg_err_t check_nsf_header( Nsf_Emu::header_t const& h )
{
	if ( !h.valid_tag() )
		return blargg_err_file_type;
	return blargg_ok;
}

struct Nsf_File : Gme_Info_
{
	Nsf_Emu::header_t const* h;
	
	Nsf_File() { set_type( gme_nsf_type ); }

	blargg_err_t load_mem_( byte const begin [], int size )
	{
		h = ( Nsf_Emu::header_t const* ) begin;

		if ( h->vers != 1 )
			set_warning( "Unknown file version" );
		
		int unsupported_chips = ~Nsf_Core::chips_mask;
		#if NSF_EMU_NO_VRC7
			unsupported_chips |= Nsf_Emu::header_t::vrc7_mask;
		#endif
		if ( h->chip_flags & unsupported_chips )
			set_warning( "Uses unsupported audio expansion hardware" );
		
		set_track_count( h->track_count );
		return check_nsf_header( *h );
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_nsf_fields( *h, out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_nsf_file( *h, file_begin() + h->size, file_end() - file_begin() - h->size, out );
		return blargg_ok;
	}
};

static Music_Emu* new_nsf_emu () { return BLARGG_NEW Nsf_Emu ; }
static Music_Emu* new_nsf_file() { return BLARGG_NEW Nsf_File; }

gme_type_t_ const gme_nsf_type [1] = {{ "Nintendo NES", 0, &new_nsf_emu, &new_nsf_file, "NSF", 1 }};

// Setup

void Nsf_Emu::set_tempo_( double t )
{
	core_.set_tempo( t );
}

void Nsf_Emu::append_voices( const char* const names [], int const types [], int count )
{
	assert( voice_count_ + count < max_voices );
	for ( int i = 0; i < count; i++ )
	{
		voice_names_ [voice_count_ + i] = names [i];
		voice_types_ [voice_count_ + i] = types [i];
	}
	voice_count_ += count;
	set_voice_count( voice_count_ );
	set_voice_types( voice_types_ );
}

blargg_err_t Nsf_Emu::init_sound()
{
	voice_count_ = 0;
	set_voice_names( voice_names_ );
	
	{
		int const count = Nes_Apu::osc_count;
		static const char* const names [Nes_Apu::osc_count] = {
			"Square 1", "Square 2", "Triangle", "Noise", "DMC"
		};
		static int const types [count] = {
			wave_type+1, wave_type+2, mixed_type+1, noise_type+0, mixed_type+1
		};
		append_voices( names, types, count );
	}
	
	// Make adjusted_gain * 0.75 = 1.0 so usual APU and one sound chip uses 1.0
	double adjusted_gain = 1.0 / 0.75 * gain();
	
#if !NSF_EMU_APU_ONLY
	// TODO: order of chips here must match that in set_voice()
	
	if ( core_.vrc6_apu() )
	{
		int const count = Nes_Vrc6_Apu::osc_count;
		static const char* const names [count] = {
			"Square 3", "Square 4", "Saw Wave"
		};
		static int const types [count] = {
			wave_type+3, wave_type+4, wave_type+5,
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.fme7_apu() )
	{
		int const count = Nes_Fme7_Apu::osc_count;
		static const char* const names [count] = {
			"Square 3", "Square 4", "Square 5"
		};
		static int const types [count] = {
			wave_type+3, wave_type+4, wave_type+5,
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.mmc5_apu() )
	{
		int const count = Nes_Mmc5_Apu::osc_count;
		static const char* const names [count] = {
			"Square 3", "Square 4", "PCM"
		};
		static int const types [count] = {
			wave_type+3, wave_type+4, mixed_type+2
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.fds_apu() )
	{
		int const count = Nes_Fds_Apu::osc_count;
		static const char* const names [count] = {
			"FM"
		};
		static int const types [count] = {
			wave_type+0
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.namco_apu() )
	{
		int const count = Nes_Namco_Apu::osc_count;
		static const char* const names [count] = {
			"Wave 1", "Wave 2", "Wave 3", "Wave 4",
			"Wave 5", "Wave 6", "Wave 7", "Wave 8"
		};
		static int const types [count] = {
			wave_type+3, wave_type+4, wave_type+5, wave_type+ 6,
			wave_type+7, wave_type+8, wave_type+9, wave_type+10,
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.vrc7_apu() )
	{
		int const count = Nes_Vrc7_Apu::osc_count;
		static const char* const names [count] = {
			"FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6"
		};
		static int const types [count] = {
			wave_type+3, wave_type+4, wave_type+5, wave_type+6,
			wave_type+7, wave_type+8
		};
		append_voices( names, types, count );
		adjusted_gain *= 0.75;
	}
	
	if ( core_.vrc7_apu()  ) core_.vrc7_apu() ->volume( adjusted_gain );
	if ( core_.namco_apu() ) core_.namco_apu()->volume( adjusted_gain );
	if ( core_.vrc6_apu()  ) core_.vrc6_apu() ->volume( adjusted_gain );
	if ( core_.fme7_apu()  ) core_.fme7_apu() ->volume( adjusted_gain );
	if ( core_.mmc5_apu()  ) core_.mmc5_apu() ->volume( adjusted_gain );
	if ( core_.fds_apu()   ) core_.fds_apu()  ->volume( adjusted_gain );
#endif
	
	if ( adjusted_gain > gain() )
		adjusted_gain = gain(); // only occurs if no other sound chips
	
	core_.nes_apu()->volume( adjusted_gain );
	
	return blargg_ok;
}

blargg_err_t Nsf_Emu::load_( Data_Reader& in )
{
	RETURN_ERR( core_.load( in ) );
	set_track_count( header().track_count );
	RETURN_ERR( check_nsf_header( header() ) );
	set_warning( core_.warning() );
	RETURN_ERR( init_sound() );
	set_tempo( tempo() );
	return setup_buffer( (int) (header().clock_rate() + 0.5) );
}

void Nsf_Emu::update_eq( blip_eq_t const& eq )
{
	core_.nes_apu()->treble_eq( eq );
	
	#if !NSF_EMU_APU_ONLY
	{
		if ( core_.namco_apu() ) core_.namco_apu()->treble_eq( eq );
		if ( core_.vrc6_apu()  ) core_.vrc6_apu() ->treble_eq( eq );
		if ( core_.fme7_apu()  ) core_.fme7_apu() ->treble_eq( eq );
		if ( core_.mmc5_apu()  ) core_.mmc5_apu() ->treble_eq( eq );
		if ( core_.fds_apu()   ) core_.fds_apu()  ->treble_eq( eq );
		if ( core_.vrc7_apu()  ) core_.vrc7_apu() ->treble_eq( eq );
	}
	#endif
}

void Nsf_Emu::set_voice( int i, Blip_Buffer* buf, Blip_Buffer*, Blip_Buffer* )
{
	#define HANDLE_CHIP( chip ) \
		if ( chip && (i -= chip->osc_count) < 0 )\
		{\
			chip->set_output( i + chip->osc_count, buf );\
			return;\
		}\
	
	HANDLE_CHIP( core_.nes_apu() );
	
	#if !NSF_EMU_APU_ONLY
	{
		// TODO: order of chips here must match that in init_sound()
		HANDLE_CHIP( core_.vrc6_apu()  );
		HANDLE_CHIP( core_.fme7_apu()  );
		HANDLE_CHIP( core_.mmc5_apu()  );
		HANDLE_CHIP( core_.fds_apu()   );
		HANDLE_CHIP( core_.namco_apu() );
		HANDLE_CHIP( core_.vrc7_apu()  );
	}
	#endif
}

blargg_err_t Nsf_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	return core_.start_track( track );
}

blargg_err_t Nsf_Emu::run_clocks( blip_time_t& duration, int )
{
	core_.end_frame( duration );
	const char* w = core_.warning();
	if ( w )
		set_warning( w );
	return blargg_ok;
}

blargg_err_t Nsf_Emu::hash_( Hash_Function& out ) const
{
	hash_nsf_file( header(), core_.rom_().begin(), core_.rom_().file_size(), out );
	return blargg_ok;
}