// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Kss_Emu.h"

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

#define IF_PTR( ptr ) if ( ptr ) (ptr)

int const clock_rate = 3579545;

#define FOR_EACH_APU( macro )\
{\
	macro( sms.psg   );\
	macro( sms.fm    );\
	macro( msx.psg   );\
	macro( msx.scc   );\
	macro( msx.music );\
	macro( msx.audio );\
}

Kss_Emu::Kss_Emu() :
	core( this )
{
	#define ACTION( apu ) { core.apu = NULL; }
	FOR_EACH_APU( ACTION );
	#undef ACTION

	set_type( gme_kss_type );
}

Kss_Emu::~Kss_Emu()
{
	unload();
}

inline void Kss_Emu::Core::unload()
{
	#define ACTION( ptr ) { delete (ptr); (ptr) = 0; }
	FOR_EACH_APU( ACTION );
	#undef ACTION
}

void Kss_Emu::unload()
{
	core.unload();
	Classic_Emu::unload();
}

// Track info

static void copy_kss_fields( Kss_Core::header_t const& h, track_info_t* out )
{
	const char* system = "MSX";

	if ( h.device_flags & 0x02 )
	{
		system = "Sega Master System";
		if ( h.device_flags & 0x04 )
			system = "Game Gear";

		if ( h.device_flags & 0x01 )
			system = "Sega Mark III";
	}
	else
	{
		if ( h.device_flags & 0x09 )
			system = "MSX + FM Sound";
	}
	Gme_File::copy_field_( out->system, system );
}

static void hash_kss_file( Kss_Core::header_t const& h, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.load_addr[0], sizeof(h.load_addr) );
	out.hash_( &h.load_size[0], sizeof(h.load_size) );
	out.hash_( &h.init_addr[0], sizeof(h.init_addr) );
	out.hash_( &h.play_addr[0], sizeof(h.play_addr) );
	out.hash_( &h.first_bank, sizeof(h.first_bank) );
	out.hash_( &h.bank_mode, sizeof(h.bank_mode) );
	out.hash_( &h.extra_header, sizeof(h.extra_header) );
	out.hash_( &h.device_flags, sizeof(h.device_flags) );

	out.hash_( data, data_size );
}

blargg_err_t Kss_Emu::track_info_( track_info_t* out, int ) const
{
	copy_kss_fields( header(), out );
// TODO: remove
//if ( msx.music ) strcpy( out->system, "msxmusic" );
//if ( msx.audio ) strcpy( out->system, "msxaudio" );
//if ( sms.fm    ) strcpy( out->system, "fmunit"   );
	return blargg_ok;
}

static blargg_err_t check_kss_header( void const* header )
{
	if ( memcmp( header, "KSCC", 4 ) && memcmp( header, "KSSX", 4 ) )
		return blargg_err_file_type;

	return blargg_ok;
}

struct Kss_File : Gme_Info_
{
	Kss_Emu::header_t const* header_;

	Kss_File() { set_type( gme_kss_type ); }

	blargg_err_t load_mem_( byte const begin [], int size )
	{
		header_ = ( Kss_Emu::header_t const* ) begin;

		if ( header_->tag [3] == 'X' && header_->extra_header == 0x10 )
			set_track_count( get_le16( header_->last_track ) + 1 );

		return check_kss_header( header_ );
	}

	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		copy_kss_fields( *header_, out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_kss_file( *header_, file_begin() + Kss_Core::header_t::base_size, file_end() - file_begin() - Kss_Core::header_t::base_size, out );
		return blargg_ok;
	}
};

static Music_Emu* new_kss_emu () { return BLARGG_NEW Kss_Emu ; }
static Music_Emu* new_kss_file() { return BLARGG_NEW Kss_File; }

gme_type_t_ const gme_kss_type [1] = {{
	"MSX",
	256,
	&new_kss_emu,
	&new_kss_file,
	"KSS",
	0x03
}};

// Setup

void Kss_Emu::Core::update_gain_()
{
	double g = emu.gain();
	if ( msx.music || msx.audio || sms.fm )
	{
		g *= 0.3;
	}
	else
	{
		g *= 1.2;
		if ( scc_accessed )
			g *= 1.4;
	}

	#define ACTION( apu ) IF_PTR( apu )->volume( g )
	FOR_EACH_APU( ACTION );
	#undef ACTION
}

static blargg_err_t new_opl_apu( Opl_Apu::type_t type, Opl_Apu** out )
{
	check( !*out );
	CHECK_ALLOC( *out = BLARGG_NEW( Opl_Apu ) );
	blip_time_t const period = 72;
	int const rate = clock_rate / period;
	return (*out)->init( rate * period, rate, period, type );
}

blargg_err_t Kss_Emu::load_( Data_Reader& in )
{
	RETURN_ERR( core.load( in ) );
	set_warning( core.warning() );

	set_track_count( get_le16( header().last_track ) + 1 );

	core.scc_enabled = false;
	if ( header().device_flags & 0x02 ) // Sega Master System
	{
		int const osc_count = Sms_Apu::osc_count + Opl_Apu::osc_count;
		static const char* const names [osc_count] = {
			"Square 1", "Square 2", "Square 3", "Noise", "FM"
		};
		set_voice_names( names );

		static int const types [osc_count] = {
			wave_type+1, wave_type+3, wave_type+2, mixed_type+1, wave_type+0
		};
		set_voice_types( types );

		// sms.psg
		set_voice_count( Sms_Apu::osc_count );
		check( !core.sms.psg );
		CHECK_ALLOC( core.sms.psg = BLARGG_NEW Sms_Apu );

		// sms.fm
		if ( header().device_flags & 0x01 )
		{
			set_voice_count( osc_count );
			RETURN_ERR( new_opl_apu( Opl_Apu::type_smsfmunit, &core.sms.fm ) );
		}

	}
	else // MSX
	{
		int const osc_count = Ay_Apu::osc_count + Opl_Apu::osc_count;
		static const char* const names [osc_count] = {
			"Square 1", "Square 2", "Square 3", "FM"
		};
		set_voice_names( names );

		static int const types [osc_count] = {
			wave_type+1, wave_type+3, wave_type+2, wave_type+0
		};
		set_voice_types( types );

		// msx.psg
		set_voice_count( Ay_Apu::osc_count );
		check( !core.msx.psg );
		CHECK_ALLOC( core.msx.psg = BLARGG_NEW Ay_Apu );

		if ( header().device_flags & 0x10 )
			set_warning( "MSX stereo not supported" );

		// msx.music
		if ( header().device_flags & 0x01 )
		{
			set_voice_count( osc_count );
			RETURN_ERR( new_opl_apu( Opl_Apu::type_msxmusic, &core.msx.music ) );
		}

		// msx.audio
		if ( header().device_flags & 0x08 )
		{
			set_voice_count( osc_count );
			RETURN_ERR( new_opl_apu( Opl_Apu::type_msxaudio, &core.msx.audio ) );
		}

		if ( !(header().device_flags & 0x80) )
		{
			if ( !(header().device_flags & 0x84) )
				core.scc_enabled = core.scc_enabled_true;

			// msx.scc
			check( !core.msx.scc );
			CHECK_ALLOC( core.msx.scc = BLARGG_NEW Scc_Apu );

			int const osc_count = Ay_Apu::osc_count + Scc_Apu::osc_count;
			static const char* const names [osc_count] = {
				"Square 1", "Square 2", "Square 3",
				"Wave 1", "Wave 2", "Wave 3", "Wave 4", "Wave 5"
			};
			set_voice_names( names );

			static int const types [osc_count] = {
				wave_type+1, wave_type+3, wave_type+2,
				wave_type+0, wave_type+4, wave_type+5, wave_type+6, wave_type+7,
			};
			set_voice_types( types );

			set_voice_count( osc_count );
		}
	}

	set_silence_lookahead( 6 );
	if ( core.sms.fm || core.msx.music || core.msx.audio )
	{
		if ( !Opl_Apu::supported() )
			set_warning( "FM sound not supported" );
		else
			set_silence_lookahead( 3 ); // Opl_Apu is really slow
	}

	return setup_buffer( ::clock_rate );
}

void Kss_Emu::update_eq( blip_eq_t const& eq )
{
	#define ACTION( apu ) IF_PTR( core.apu )->treble_eq( eq )
	FOR_EACH_APU( ACTION );
	#undef ACTION
}

void Kss_Emu::set_voice( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	if ( core.sms.psg ) // Sega Master System
	{
		i -= core.sms.psg->osc_count;
		if ( i < 0 )
		{
			core.sms.psg->set_output( i + core.sms.psg->osc_count, center, left, right );
			return;
		}

		if ( core.sms.fm && i < core.sms.fm->osc_count )
			core.sms.fm->set_output( i, center, NULL, NULL );
	}
	else if ( core.msx.psg ) // MSX
	{
		i -= core.msx.psg->osc_count;
		if ( i < 0 )
		{
			core.msx.psg->set_output( i + core.msx.psg->osc_count, center );
			return;
		}

		if ( core.msx.scc   && i < core.msx.scc->osc_count   ) core.msx.scc  ->set_output( i, center );
		if ( core.msx.music && i < core.msx.music->osc_count ) core.msx.music->set_output( i, center, NULL, NULL );
		if ( core.msx.audio && i < core.msx.audio->osc_count ) core.msx.audio->set_output( i, center, NULL, NULL );
	}
}

void Kss_Emu::set_tempo_( double t )
{
	int period = (header().device_flags & 0x40 ? ::clock_rate / 50 : ::clock_rate / 60);
	core.set_play_period( (Kss_Core::time_t) (period / t) );
}

blargg_err_t Kss_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );

	#define ACTION( apu ) IF_PTR( core.apu )->reset()
	FOR_EACH_APU( ACTION );
	#undef ACTION

	core.scc_accessed = false;
	core.update_gain_();

	return core.start_track( track );
}

void Kss_Emu::Core::cpu_write_( addr_t addr, int data )
{
	// TODO: SCC+ support

	data &= 0xFF;
	switch ( addr )
	{
	case 0x9000:
		set_bank( 0, data );
		return;

	case 0xB000:
		set_bank( 1, data );
		return;

	case 0xBFFE: // selects between mapping areas (we just always enable both)
		if ( data == 0 || data == 0x20 )
			return;
	}

	int scc_addr = (addr & 0xDFFF) - 0x9800;
	if ( (unsigned) scc_addr < 0xB0 && msx.scc )
	{
		scc_accessed = true;
		//if ( (unsigned) (scc_addr - 0x90) < 0x10 )
		//  scc_addr -= 0x10; // 0x90-0x9F mirrors to 0x80-0x8F
		if ( scc_addr < Scc_Apu::reg_count )
			msx.scc->write( cpu.time(), addr, data );
		return;
	}

	dprintf( "LD ($%04X),$%02X\n", addr, data );
}

void Kss_Emu::Core::cpu_write( addr_t addr, int data )
{
	*cpu.write( addr ) = data;
	if ( (addr & scc_enabled) == 0x8000 )
		cpu_write_( addr, data );
}

void Kss_Emu::Core::cpu_out( time_t time, addr_t addr, int data )
{
	data &= 0xFF;
	switch ( addr & 0xFF )
	{
	case 0xA0:
		if ( msx.psg )
			msx.psg->write_addr( data );
		return;

	case 0xA1:
		if ( msx.psg )
			msx.psg->write_data( time, data );
		return;

	case 0x06:
		if ( sms.psg && (header().device_flags & 0x04) )
		{
			sms.psg->write_ggstereo( time, data );
			return;
		}
		break;

	case 0x7E:
	case 0x7F:
		if ( sms.psg )
		{
			sms.psg->write_data( time, data );
			return;
		}
		break;

	#define OPL_WRITE_HANDLER( base, opl )\
		case base  : if ( opl ) { opl->write_addr(       data ); return; } break;\
		case base+1: if ( opl ) { opl->write_data( time, data ); return; } break;

	OPL_WRITE_HANDLER( 0x7C, msx.music )
	OPL_WRITE_HANDLER( 0xC0, msx.audio )
	OPL_WRITE_HANDLER( 0xF0, sms.fm    )

	case 0xFE:
		set_bank( 0, data );
		return;

	#ifndef NDEBUG
	case 0xA8: // PPI
		return;
	#endif
	}

	Kss_Core::cpu_out( time, addr, data );
}

int Kss_Emu::Core::cpu_in( time_t time, addr_t addr )
{
	switch ( addr & 0xFF )
	{
	case 0xC0:
	case 0xC1:
		if ( msx.audio )
			return msx.audio->read( time, addr & 1 );
		break;

	case 0xA2:
		if ( msx.psg )
			return msx.psg->read();
		break;

	#ifndef NDEBUG
	case 0xA8: // PPI
		return 0;
	#endif
	}

	return Kss_Core::cpu_in( time, addr );
}

void Kss_Emu::Core::update_gain()
{
	if ( scc_accessed )
	{
		dprintf( "SCC accessed\n" );
		update_gain_();
	}
}

blargg_err_t Kss_Emu::run_clocks( blip_time_t& duration, int )
{
	RETURN_ERR( core.end_frame( duration ) );

	#define ACTION( apu ) IF_PTR( core.apu )->end_frame( duration )
	FOR_EACH_APU( ACTION );
	#undef ACTION

	return blargg_ok;
}

blargg_err_t Kss_Emu::hash_( Hash_Function& out ) const
{
	hash_kss_file( header(), core.rom_().begin(), core.rom_().file_size(), out );
	return blargg_ok;
}