// Nintendo Game Boy GBS music file emulator

// Game_Music_Emu $vers
#ifndef GBS_EMU_H
#define GBS_EMU_H

#include "Classic_Emu.h"
#include "Gbs_Core.h"

class Gbs_Emu : public Classic_Emu {
public:
	// Equalizer profiles for Game Boy speaker and headphones
	static equalizer_t const handheld_eq;
	static equalizer_t const headphones_eq;
	static equalizer_t const cgb_eq; // Game Boy Color headphones have less bass
	
	// GBS file header (see Gbs_Core.h)
	typedef Gbs_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const { return core_.header(); }
	
	// Selects which sound hardware to use. AGB hardware is cleaner than the
	// others. Doesn't take effect until next start_track().
	enum sound_t {
		sound_dmg = Gb_Apu::mode_dmg,   // Game Boy monochrome
		sound_cgb = Gb_Apu::mode_cgb,   // Game Boy Color
		sound_agb = Gb_Apu::mode_agb,   // Game Boy Advance
		sound_gbs                       // Use DMG/CGB based on GBS (default)
	};
	void set_sound( sound_t s ) { sound_hardware = s; }
	
	// If true, makes APU more accurate, which results in more clicking.
	void enable_clicking( bool enable = true ) { core_.apu().reduce_clicks( !enable ); }
	
	static gme_type_t static_type() { return gme_gbs_type; }

	Gbs_Core& core() { return core_; }

	blargg_err_t hash_( Hash_Function& ) const;
	
// Internal
public:
	Gbs_Emu();
	~Gbs_Emu();

protected:
	// Overrides
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_( Data_Reader& );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );
	virtual void unload();

private:
	sound_t sound_hardware;
	Gbs_Core core_;
};

#endif
