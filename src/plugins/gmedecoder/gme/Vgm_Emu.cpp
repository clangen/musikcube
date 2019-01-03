// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Vgm_Emu.h"

#include "blargg_endian.h"
#include "blargg_common.h"

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

// FM emulators are internally quieter to avoid 16-bit overflow
double const fm_gain           = 3.0;
double const rolloff           = 0.990;
double const oversample_factor = 1.5;

Vgm_Emu::Vgm_Emu()
{
	resampler.set_callback( play_frame_, this );
	disable_oversampling_ = false;
	muted_voices = 0;
	set_type( gme_vgm_type );
	set_max_initial_silence( 1 );
	set_silence_lookahead( 1 ); // tracks should already be trimmed

	static equalizer_t const eq = { -14.0, 80 , 0,0,0,0,0,0,0,0 };
	set_equalizer( eq );
}

Vgm_Emu::~Vgm_Emu() { }

void Vgm_Emu::unload()
{
	core.unload();
	Classic_Emu::unload();
}

// Track info

static byte const* skip_gd3_str( byte const in [], byte const* end )
{
	while ( end - in >= 2 )
	{
		in += 2;
		if ( !(in [-2] | in [-1]) )
			break;
	}
	return in;
}

static byte const* get_gd3_str( byte const* in, byte const* end, char field [] )
{
	byte const* mid = skip_gd3_str( in, end );
	int len = (int)((mid - in) / 2) - 1;
	if ( len > 0 )
	{
        char * in_utf8 = blargg_to_utf8( (blargg_wchar_t *) in );
		len = min( len, (int) Gme_File::max_field_ );
		field [len] = 0;
		for ( int i = 0; i < len; i++ )
            field [i] = in_utf8 [i];
        free(in_utf8);
	}
	return mid;
}

static byte const* get_gd3_pair( byte const* in, byte const* end, char field [] )
{
	return skip_gd3_str( get_gd3_str( in, end, field ), end );
}

static void parse_gd3( byte const in [], byte const* end, track_info_t* out )
{
	in = get_gd3_pair( in, end, out->song      );
	in = get_gd3_pair( in, end, out->game      );
	in = get_gd3_pair( in, end, out->system    );
	in = get_gd3_pair( in, end, out->author    );
	in = get_gd3_str ( in, end, out->copyright );
	in = get_gd3_pair( in, end, out->dumper    );
	in = get_gd3_str ( in, end, out->comment   );
}

int const gd3_header_size = 12;

static int check_gd3_header( byte const h [], int remain )
{
	if ( remain < gd3_header_size ) return 0;
	if ( memcmp( h, "Gd3 ", 4 ) ) return 0;
	if ( get_le32( h + 4 ) >= 0x200 ) return 0;
	
	int gd3_size = get_le32( h + 8 );
	if ( gd3_size > remain - gd3_header_size ) return 0;
	
	return gd3_size;
}

static void get_vgm_length( Vgm_Emu::header_t const& h, track_info_t* out )
{
	int length = get_le32( h.track_duration ) * 10 / 441; // 1000 / 44100
	if ( length > 0 )
	{
		int loop = get_le32( h.loop_duration );
		if ( loop > 0 && get_le32( h.loop_offset ) )
		{
			out->loop_length  = loop * 10 / 441;
			out->intro_length = length - out->loop_length;
			check( out->loop_length <= length );
			// TODO: Also set out->length? We now have play_length for suggested play time.
		}
		else
		{
			out->length       = length;
			out->intro_length = length;
			out->loop_length  = 0;
		}
	}
}

blargg_err_t Vgm_Emu::track_info_( track_info_t* out, int ) const
{
	get_vgm_length( header(), out );
	
	int gd3_offset = get_le32( header().gd3_offset );
	if ( gd3_offset <= 0 )
		return blargg_ok;
	
	byte const* gd3 = core.file_begin() + gd3_offset + offsetof( header_t, gd3_offset );
	int gd3_size = check_gd3_header( gd3, core.file_end() - gd3 );
	if ( gd3_size )
	{
		byte const* gd3_data = gd3 + gd3_header_size;
		parse_gd3( gd3_data, gd3_data + gd3_size, out );
	}
	
	return blargg_ok;
}

blargg_err_t Vgm_Emu::gd3_data( const unsigned char ** data, int * size )
{
	*data = 0;
	*size = 0;

	int gd3_offset = get_le32( header().gd3_offset );
	if ( gd3_offset <= 0 )
		return blargg_ok;

	byte const* gd3 = core.file_begin() + gd3_offset + offsetof( header_t, gd3_offset );
	int gd3_size = check_gd3_header( gd3, core.file_end() - gd3 );
	if ( gd3_size )
	{
		*data = gd3;
		*size = gd3_size + gd3_header_size;
	}

	return blargg_ok;
}

static void hash_vgm_file( Vgm_Emu::header_t const& h, byte const* data, int data_size, Music_Emu::Hash_Function& out )
{
	out.hash_( &h.data_size[0], sizeof(h.data_size) );
	out.hash_( &h.version[0], sizeof(h.version) );
	out.hash_( &h.psg_rate[0], sizeof(h.psg_rate) );
	out.hash_( &h.ym2413_rate[0], sizeof(h.ym2413_rate) );
	out.hash_( &h.track_duration[0], sizeof(h.track_duration) );
	out.hash_( &h.loop_offset[0], sizeof(h.loop_offset) );
	out.hash_( &h.loop_duration[0], sizeof(h.loop_duration) );
	out.hash_( &h.frame_rate[0], sizeof(h.frame_rate) );
	out.hash_( &h.noise_feedback[0], sizeof(h.noise_feedback) );
	out.hash_( &h.noise_width, sizeof(h.noise_width) );
	out.hash_( &h.sn76489_flags, sizeof(h.sn76489_flags) );
	out.hash_( &h.ym2612_rate[0], sizeof(h.ym2612_rate) );
	out.hash_( &h.ym2151_rate[0], sizeof(h.ym2151_rate) );
	out.hash_( &h.data_offset[0], sizeof(h.data_offset) );
	out.hash_( &h.segapcm_rate[0], sizeof(h.segapcm_rate) );
	out.hash_( &h.segapcm_reg[0], sizeof(h.segapcm_reg) );
	out.hash_( &h.rf5c68_rate[0], sizeof(h.rf5c68_rate) );
	out.hash_( &h.ym2203_rate[0], sizeof(h.ym2203_rate) );
	out.hash_( &h.ym2608_rate[0], sizeof(h.ym2608_rate) );
	out.hash_( &h.ym2610_rate[0], sizeof(h.ym2610_rate) );
	out.hash_( &h.ym3812_rate[0], sizeof(h.ym3812_rate) );
	out.hash_( &h.ym3526_rate[0], sizeof(h.ym3526_rate) );
	out.hash_( &h.y8950_rate[0], sizeof(h.y8950_rate) );
	out.hash_( &h.ymf262_rate[0], sizeof(h.ymf262_rate) );
	out.hash_( &h.ymf278b_rate[0], sizeof(h.ymf278b_rate) );
	out.hash_( &h.ymf271_rate[0], sizeof(h.ymf271_rate) );
	out.hash_( &h.ymz280b_rate[0], sizeof(h.ymz280b_rate) );
	out.hash_( &h.rf5c164_rate[0], sizeof(h.rf5c164_rate) );
	out.hash_( &h.pwm_rate[0], sizeof(h.pwm_rate) );
	out.hash_( &h.ay8910_rate[0], sizeof(h.ay8910_rate) );
	out.hash_( &h.ay8910_type, sizeof(h.ay8910_type) );
	out.hash_( &h.ay8910_flags, sizeof(h.ay8910_flags) );
	out.hash_( &h.ym2203_ay8910_flags, sizeof(h.ym2203_ay8910_flags) );
	out.hash_( &h.ym2608_ay8910_flags, sizeof(h.ym2608_ay8910_flags) );
	out.hash_( &h.reserved, sizeof(h.reserved) );
	out.hash_( &h.gbdmg_rate[0], sizeof(h.gbdmg_rate) );
	out.hash_( &h.nesapu_rate[0], sizeof(h.nesapu_rate) );
	out.hash_( &h.multipcm_rate[0], sizeof(h.multipcm_rate) );
	out.hash_( &h.upd7759_rate[0], sizeof(h.upd7759_rate) );
	out.hash_( &h.okim6258_rate[0], sizeof(h.okim6258_rate) );
	out.hash_( &h.okim6258_flags, sizeof(h.okim6258_flags) );
	out.hash_( &h.k054539_flags, sizeof(h.k054539_flags) );
	out.hash_( &h.c140_type, sizeof(h.c140_type) );
	out.hash_( &h.reserved_flags, sizeof(h.reserved_flags) );
	out.hash_( &h.okim6295_rate[0], sizeof(h.okim6295_rate) );
	out.hash_( &h.k051649_rate[0], sizeof(h.k051649_rate) );
	out.hash_( &h.k054539_rate[0], sizeof(h.k054539_rate) );
	out.hash_( &h.huc6280_rate[0], sizeof(h.huc6280_rate) );
	out.hash_( &h.c140_rate[0], sizeof(h.c140_rate) );
	out.hash_( &h.k053260_rate[0], sizeof(h.k053260_rate) );
	out.hash_( &h.pokey_rate[0], sizeof(h.pokey_rate) );
	out.hash_( &h.qsound_rate[0], sizeof(h.qsound_rate) );
	out.hash_( &h.reserved2[0], sizeof(h.reserved2) );
	out.hash_( &h.extra_offset[0], sizeof(h.extra_offset) );
	out.hash_( data, data_size );
}

struct Vgm_File : Gme_Info_
{
	Vgm_Emu::header_t h;
	blargg_vector<byte> data;
	blargg_vector<byte> gd3;
	
	Vgm_File() { set_type( gme_vgm_type ); }
	
	blargg_err_t load_( Data_Reader& in )
	{
		int file_size = in.remain();
		if ( file_size <= h.size_min )
			return blargg_err_file_type;
		
		RETURN_ERR( in.read( &h, h.size_min ) );
		if ( !h.valid_tag() )
			return blargg_err_file_type;

		if ( h.size() > h.size_min )
			RETURN_ERR( in.read( &h.rf5c68_rate, h.size() - h.size_min ) );

		h.cleanup();

		int data_offset = get_le32( h.data_offset ) + offsetof( Vgm_Core::header_t, data_offset );
		int data_size = file_size - offsetof( Vgm_Core::header_t, data_offset ) - data_offset;
		int gd3_offset = get_le32( h.gd3_offset );
		if ( gd3_offset > 0 )
			gd3_offset += offsetof( Vgm_Core::header_t, gd3_offset );

		int amount_to_skip = gd3_offset - h.size();

		if ( gd3_offset > 0 && gd3_offset > data_offset )
		{
			data_size = gd3_offset - data_offset;
			amount_to_skip = 0;

			RETURN_ERR( data.resize( data_size ) );
			RETURN_ERR( in.skip( data_offset - h.size() ) );
			RETURN_ERR( in.read( data.begin(), data_size ) );
		}

		int remain = file_size - gd3_offset;
		byte gd3_h [gd3_header_size];
		if ( gd3_offset > 0 && remain >= gd3_header_size )
		{
			RETURN_ERR( in.skip( amount_to_skip ) );
			RETURN_ERR( in.read( gd3_h, sizeof gd3_h ) );
			int gd3_size = check_gd3_header( gd3_h, remain );
			if ( gd3_size )
			{
				RETURN_ERR( gd3.resize( gd3_size ) );
				RETURN_ERR( in.read( gd3.begin(), gd3.size() ) );
			}

			if ( data_offset > gd3_offset )
			{
				RETURN_ERR( data.resize( data_size ) );
				RETURN_ERR( in.skip( data_offset - gd3_offset - sizeof gd3_h - gd3.size() ) );
				RETURN_ERR( in.read( data.begin(), data.end() - data.begin() ) );
			}
		}

		return blargg_ok;
	}
	
	blargg_err_t track_info_( track_info_t* out, int ) const
	{
		get_vgm_length( h, out );
		if ( gd3.size() )
			parse_gd3( gd3.begin(), gd3.end(), out );
		return blargg_ok;
	}

	blargg_err_t hash_( Hash_Function& out ) const
	{
		hash_vgm_file( h, data.begin(), data.end() - data.begin(), out );
		return blargg_ok;
	}
};

static Music_Emu* new_vgm_emu () { return BLARGG_NEW Vgm_Emu ; }
static Music_Emu* new_vgm_file() { return BLARGG_NEW Vgm_File; }

gme_type_t_ const gme_vgm_type [1] = {{ "Sega SMS/Genesis", 1, &new_vgm_emu, &new_vgm_file, "VGM", 1 }};

gme_type_t_ const gme_vgz_type [1] = {{ "Sega SMS/Genesis", 1, &new_vgm_emu, &new_vgm_file, "VGZ", 1 }};

// Setup

void Vgm_Emu::set_tempo_( double t )
{
	core.set_tempo( t );
}

blargg_err_t Vgm_Emu::set_sample_rate_( int sample_rate )
{
	RETURN_ERR( core.stereo_buf[0].set_sample_rate( sample_rate, 1000 / 30 ) );
	RETURN_ERR( core.stereo_buf[1].set_sample_rate( sample_rate, 1000 / 30 ) );
    RETURN_ERR( core.stereo_buf[2].set_sample_rate( sample_rate, 1000 / 30 ) );
	RETURN_ERR( core.stereo_buf[3].set_sample_rate( sample_rate, 1000 / 30 ) );
	core.set_sample_rate(sample_rate);
	return Classic_Emu::set_sample_rate_( sample_rate );
}

void Vgm_Emu::update_eq( blip_eq_t const& eq )
{
	core.psg[0].treble_eq( eq );
	core.psg[1].treble_eq( eq );
	core.ay[0].treble_eq( eq );
	core.ay[1].treble_eq( eq );
    core.huc6280[0].treble_eq( eq );
    core.huc6280[1].treble_eq( eq );
	core.gbdmg[0].treble_eq( eq );
	core.gbdmg[1].treble_eq( eq );
	core.pcm.treble_eq( eq );
}

void Vgm_Emu::set_voice( int i, Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	if ( i < core.psg[0].osc_count )
	{
		core.psg[0].set_output( i, c, l, r );
		core.psg[1].set_output( i, c, l, r );
	}
}

void Vgm_Emu::mute_voices_( int mask )
{
	muted_voices = mask;

	Classic_Emu::mute_voices_( mask );
	
	// TODO: what was this for?
	//core.pcm.output( &core.blip_buf );
	
	// TODO: silence PCM if FM isn't used?
	if ( core.uses_fm() )
	{
		core.psg[0].set_output( ( mask & 0x80 ) ? 0 : core.stereo_buf[0].center() );
		core.psg[1].set_output( ( mask & 0x80 ) ? 0 : core.stereo_buf[0].center() );
		core.ay[0].set_output( ( mask & 0x80 ) ? 0 : core.stereo_buf[1].center() );
		core.ay[1].set_output( ( mask & 0x80 ) ? 0 : core.stereo_buf[1].center() );
        for ( unsigned i = 0, j = 1; i < core.huc6280[0].osc_count; i++, j <<= 1)
        {
            Blip_Buffer * center = ( mask & j ) ? 0 : core.stereo_buf[2].center();
            Blip_Buffer * left   = ( mask & j ) ? 0 : core.stereo_buf[2].left();
            Blip_Buffer * right  = ( mask & j ) ? 0 : core.stereo_buf[2].right();
            core.huc6280[0].set_output( i, center, left, right );
            core.huc6280[1].set_output( i, center, left, right );
        }
		for (unsigned i = 0, j = 1; i < core.gbdmg[0].osc_count; i++, j <<= 1)
		{
			Blip_Buffer * center = (mask & j) ? 0 : core.stereo_buf[3].center();
			Blip_Buffer * left   = (mask & j) ? 0 : core.stereo_buf[3].left();
			Blip_Buffer * right  = (mask & j) ? 0 : core.stereo_buf[3].right();
			core.gbdmg[0].set_output(i, center, left, right);
			core.gbdmg[1].set_output(i, center, left, right);
		}
		if (core.ym2612[0].enabled())
		{
			core.pcm.volume( (mask & 0x40) ? 0.0 : 0.1115 / 256 * fm_gain * gain() );
			core.ym2612[0].mute_voices( mask );
			if ( core.ym2612[1].enabled() )
				core.ym2612[1].mute_voices( mask );
		}
		
		if ( core.ym2413[0].enabled() )
		{
			int m = mask & 0x3F;
			if ( mask & 0x20 )
				m |= 0x01E0; // channels 5-8
			if ( mask & 0x40 )
				m |= 0x3E00;
			core.ym2413[0].mute_voices( m );
			if ( core.ym2413[1].enabled() )
				core.ym2413[1].mute_voices( m );
		}

		if ( core.ym2151[0].enabled() )
		{
			core.ym2151[0].mute_voices( mask );
			if ( core.ym2151[1].enabled() )
				core.ym2151[1].mute_voices( mask );
		}

		if ( core.c140.enabled() )
		{
			int m = 0;
			int m_add = 7;
			for ( unsigned i = 0; i < 8; i++, m_add <<= 3 )
			{
				if ( mask & ( 1 << i ) ) m += m_add;
			}
			core.c140.mute_voices( m );
		}

		if ( core.rf5c68.enabled() )
		{
			core.rf5c68.mute_voices( mask );
		}

		if ( core.rf5c164.enabled() )
		{
			core.rf5c164.mute_voices( mask );
		}
	}
}

blargg_err_t Vgm_Emu::load_mem_( byte const data [], int size )
{
	RETURN_ERR( core.load_mem( data, size ) );

	set_voice_count( core.psg[0].osc_count );
	
	double fm_rate = 0.0;
	if ( !disable_oversampling_ )
		fm_rate = sample_rate() * oversample_factor;
	RETURN_ERR( core.init_chips( &fm_rate ) );

	double psg_gain = ( ( core.header().psg_rate[3] & 0xC0 ) == 0x40 ) ? 0.5 : 1.0;
	
	if ( core.uses_fm() )
	{
		set_voice_count( 8 );
		RETURN_ERR( resampler.setup( fm_rate / sample_rate(), rolloff, gain() ) );
		RETURN_ERR( resampler.reset( core.stereo_buf[0].length() * sample_rate() / 1000 ) );
		core.psg[0].volume( 0.135 * fm_gain * psg_gain * gain() );
		core.psg[1].volume( 0.135 * fm_gain * psg_gain * gain() );
		core.ay[0].volume( 0.135 * fm_gain * gain() );
		core.ay[1].volume( 0.135 * fm_gain * gain() );
        core.huc6280[0].volume( 0.135 * fm_gain * gain() );
        core.huc6280[1].volume( 0.135 * fm_gain * gain() );
		core.gbdmg[0].volume( 0.135 * fm_gain * gain() );
		core.gbdmg[1].volume( 0.135 * fm_gain * gain() );
	}
	else
	{
		core.psg[0].volume( psg_gain * gain() );
		core.psg[1].volume( psg_gain * gain() );
	}
	
	static const char* const fm_names [] = {
		"FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6", "PCM", "PSG"
	};
	static const char* const psg_names [] = { "Square 1", "Square 2", "Square 3", "Noise" };
	set_voice_names( core.uses_fm() ? fm_names : psg_names );
	
	static int const types [8] = {
		wave_type+1, wave_type+2, wave_type+3, noise_type+1,
		0, 0, 0, 0
	};
	set_voice_types( types );
	
	return Classic_Emu::setup_buffer( core.stereo_buf[0].center()->clock_rate() );
}

// Emulation

blargg_err_t Vgm_Emu::start_track_( int track )
{
	RETURN_ERR( Classic_Emu::start_track_( track ) );
	
	core.start_track();

	mute_voices_(muted_voices);
	
	if ( core.uses_fm() )
		resampler.clear();
	
	return blargg_ok;
}

inline void Vgm_Emu::check_end()
{
	if ( core.track_ended() )
		set_track_ended();
}

inline void Vgm_Emu::check_warning()
{
	const char* w = core.warning();
	if ( w )
		set_warning( w );
}

blargg_err_t Vgm_Emu::run_clocks( blip_time_t& time_io, int msec )
{
	check_end();
	time_io = core.run_psg( msec );
	check_warning();
	return blargg_ok;
}

inline int Vgm_Emu::play_frame( blip_time_t blip_time, int sample_count, sample_t buf [] )
{
	check_end();
	int result = core.play_frame( blip_time, sample_count, buf );
	check_warning();
	return result;
}

int Vgm_Emu::play_frame_( void* p, blip_time_t a, int b, sample_t c [] )
{
	return STATIC_CAST(Vgm_Emu*,p)->play_frame( a, b, c );
}

blargg_err_t Vgm_Emu::play_( int count, sample_t out [] )
{
	if ( !core.uses_fm() )
		return Classic_Emu::play_( count, out );

    Stereo_Buffer * secondaries[] = { &core.stereo_buf[1], &core.stereo_buf[2], &core.stereo_buf[3] };
    resampler.dual_play( count, out, core.stereo_buf[0], secondaries, 3 );
	return blargg_ok;
}

blargg_err_t Vgm_Emu::hash_( Hash_Function& out ) const
{
	byte const* p = file_begin() + header().size();
	byte const* e = file_end();
	int data_offset = get_le32( header().data_offset );
	if ( data_offset )
		p += data_offset + offsetof( header_t, data_offset ) - header().size();
	int gd3_offset = get_le32( header().gd3_offset );
	if ( gd3_offset > 0 && gd3_offset + offsetof( header_t, gd3_offset ) > data_offset + offsetof( header_t, data_offset ) )
		e = file_begin() + gd3_offset + offsetof( header_t, gd3_offset );
	hash_vgm_file( header(), p, e - p, out );
	return blargg_ok;
}
