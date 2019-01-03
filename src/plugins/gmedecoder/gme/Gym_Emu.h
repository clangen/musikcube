// Sega Genesis/Mega Drive GYM music file emulator
// Performs PCM timing recovery to improve sample quality.

// Game_Music_Emu $vers
#ifndef GYM_EMU_H
#define GYM_EMU_H

#include "Dual_Resampler.h"
#include "Ym2612_Emu.h"
#include "Music_Emu.h"
#include "Sms_Apu.h"

class Gym_Emu : public Music_Emu {
public:
	
	// GYM file header (optional; many files have NO header at all)
	struct header_t
	{
		enum { size = 428 };
		
	    char tag        [  4];
	    char song       [ 32];
	    char game       [ 32];
	    char copyright  [ 32];
	    char emulator   [ 32];
	    char dumper     [ 32];
	    char comment    [256];
	    byte loop_start [  4]; // in 1/60 seconds, 0 if not looped
	    byte packed     [  4];
	};
	
	// Header for currently loaded file
	header_t const& header() const          { return header_; }
	
	static gme_type_t static_type()         { return gme_gym_type; }
	
	// Disables running FM chips at higher than normal rate. Will result in slightly
	// more aliasing of high notes.
	void disable_oversampling( bool disable = true ) { disable_oversampling_ = disable; }

	blargg_err_t hash_( Hash_Function& ) const;
	
// Implementation
public:
	Gym_Emu();
	~Gym_Emu();

protected:
	virtual blargg_err_t load_mem_( byte const [], int );
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t set_sample_rate_( int sample_rate );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t play_( int count, sample_t [] );
	virtual void mute_voices_( int );
	virtual void set_tempo_( double );

private:
	// Log
	byte const* pos;        // current position
	byte const* loop_begin;
	int log_offset;         // size of header (0 or header_t::size)
	int loop_remain;        // frames remaining until loop_begin has been located
	int clocks_per_frame;

	bool disable_oversampling_;
	
	// PCM
	int pcm_amp;
	int prev_pcm_count;     // for detecting beginning/end of group of samples
	int pcm_enabled;
	
	// large objects
	Dual_Resampler  resampler;
	Stereo_Buffer   stereo_buf;
	Blip_Buffer   * pcm_buf;
	Ym2612_Emu      fm;
	Sms_Apu         apu;
	Blip_Synth_Fast pcm_synth;
	header_t        header_;
	
	byte const* log_begin() const { return file_begin() + log_offset; }
	void parse_frame();
	void run_pcm( byte const in [], int count );
	int play_frame( blip_time_t blip_time, int sample_count, sample_t buf [] );
	static int play_frame_( void*, blip_time_t, int, sample_t [] );
};

#endif
