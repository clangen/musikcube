// Sega VGM music file emulator

// Game_Music_Emu $vers
#ifndef VGM_EMU_H
#define VGM_EMU_H

#include "Classic_Emu.h"
#include "Dual_Resampler.h"
#include "Vgm_Core.h"

/* Emulates VGM music using SN76489/SN76496 PSG, and YM2612 and YM2413 FM sound chips.
Supports custom sound buffer and frequency equalization when VGM uses just the PSG. FM
sound chips can be run at their proper rates, or slightly higher to reduce aliasing on
high notes. A YM2413 is supported but not provided separately from the library. */
class Vgm_Emu : public Classic_Emu {
public:

	// True if custom buffer and custom equalization are supported
	// TODO: move into Music_Emu and rename to something like supports_custom_buffer()
	bool is_classic_emu() const                         { return !core.uses_fm(); }
	
	// Disables running FM chips at higher than normal rate. Will result in slightly
	// more aliasing of high notes.
	void disable_oversampling( bool disable = true )    { disable_oversampling_ = disable; }
	
	// VGM file header (see Vgm_Core.h)
	typedef Vgm_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const                      { return core.header(); }

	blargg_err_t hash_( Hash_Function& ) const;

	// Gd3 tag for currently loaded file
	blargg_err_t gd3_data( const unsigned char ** data, int * size );
	
	static gme_type_t static_type()                     { return gme_vgm_type; }

// Implementation
public:
	Vgm_Emu();
	~Vgm_Emu();
	
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_mem_( byte const [], int );
	blargg_err_t set_sample_rate_( int sample_rate );
	blargg_err_t start_track_( int );
	blargg_err_t play_( int count, sample_t  []);
	blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void mute_voices_( int mask );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );
	virtual void unload();
	
private:
	bool disable_oversampling_;
	unsigned muted_voices;
	Dual_Resampler resampler;
	Vgm_Core core;
	
	void check_end();
	void check_warning();
	int play_frame( blip_time_t blip_time, int sample_count, sample_t buf [] );
	static int play_frame_( void*, blip_time_t, int, sample_t [] );
};

#endif
