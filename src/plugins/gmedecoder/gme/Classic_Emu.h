// Common aspects of emulators which use Blip_Buffer for sound output

// Game_Music_Emu $vers
#ifndef CLASSIC_EMU_H
#define CLASSIC_EMU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"
#include "Music_Emu.h"

class Classic_Emu : public Music_Emu {
protected:
// Derived interface

	// Advertises type of sound on each voice, so Effects_Buffer can better choose
	// what effect to apply (pan, echo, surround). Constant can have value added so
	// that voices of the same type can be spread around the stereo sound space.
	enum { wave_type = 0x100, noise_type = 0x200, mixed_type = wave_type | noise_type };
	void set_voice_types( int const types [] )          { voice_types = types; }
	
	// Sets up Blip_Buffers after loading file
	blargg_err_t setup_buffer( int clock_rate );
	
	// Clock rate of Blip_buffers
	int  clock_rate() const                             { return clock_rate_; }
	
	// Changes clock rate of Blip_Buffers (experimental)
	void change_clock_rate( int );
	
// Overrides should do the indicated task
	
	// Set Blip_Buffer(s) voice outputs to, or mute voice if pointer is NULL
	virtual void set_voice( int index, Blip_Buffer* center,
			Blip_Buffer* left, Blip_Buffer* right )                     BLARGG_PURE( ; )
	
	// Update equalization
	virtual void update_eq( blip_eq_t const& )                          BLARGG_PURE( ; )
	
	// Start track
	virtual blargg_err_t start_track_( int track )                      BLARGG_PURE( ; )
	
	// Run for at most msec or time_io clocks, then set time_io to number of clocks
	// actually run for. After returning, Blip_Buffers have time frame of time_io clocks
	// ended.
	virtual blargg_err_t run_clocks( blip_time_t& time_io, int msec )   BLARGG_PURE( ; )

// Internal
public:
	Classic_Emu();
	~Classic_Emu();
	virtual void set_buffer( Multi_Buffer* );

protected:
	virtual blargg_err_t set_sample_rate_( int sample_rate );
	virtual void mute_voices_( int );
	virtual void set_equalizer_( equalizer_t const& );
	virtual blargg_err_t play_( int, sample_t [] );

private:
	Multi_Buffer* buf;
	Multi_Buffer* stereo_buffer; // NULL if using custom buffer
	int clock_rate_;
	unsigned buf_changed_count;
	int const* voice_types;
};

inline void Classic_Emu::set_buffer( Multi_Buffer* new_buf )
{
	assert( !buf && new_buf );
	buf = new_buf;
}

inline void Classic_Emu::set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* ) { }

inline void Classic_Emu::update_eq( blip_eq_t const& )                              { }

inline blargg_err_t Classic_Emu::run_clocks( blip_time_t&, int )                    { return blargg_ok; }

#endif
