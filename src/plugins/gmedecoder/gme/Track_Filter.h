// Removes silence from beginning of track, fades end of track. Also looks ahead
// for excessive silence, and if found, ends track.

// Game_Music_Emu $vers
#ifndef TRACK_FILTER_H
#define TRACK_FILTER_H

#include "blargg_common.h"

class Track_Filter {
public:
	typedef int sample_count_t;
	typedef short sample_t;
	
	enum { indefinite_count = INT_MAX/2 + 1 };
	
	struct callbacks_t {
		// Samples may be stereo or mono
		virtual blargg_err_t play_( int count, sample_t* out )  BLARGG_PURE( { return blargg_ok; } )
		virtual blargg_err_t skip_( int count )                 BLARGG_PURE( { return blargg_ok; } )
		virtual ~callbacks_t() { } // avoids silly "non-virtual dtor" warning
	};
	
	// Initializes filter. Must be done once before using object.
	blargg_err_t init( callbacks_t* );
	
	struct setup_t {
		sample_count_t max_initial; // maximum silence to strip from beginning of track
		sample_count_t max_silence; // maximum silence in middle of track without it ending
		int lookahead;   // internal speed when looking ahead for silence (2=200% etc.)
	};
	
	// Gets/sets setup
	setup_t const& setup() const                { return setup_; }
	void setup( setup_t const& s )              { setup_ = s; }
	
	// Disables automatic end-of-track detection and skipping of silence at beginning
	void ignore_silence( bool disable = true )  { silence_ignored_ = disable; }
	
	// Clears state and skips initial silence in track
	blargg_err_t start_track();
	
	// Sets time that fade starts, and how long until track ends.
	void set_fade( sample_count_t start, sample_count_t length );
	
	// Generates n samples into buf
	blargg_err_t play( int n, sample_t buf [] );
	
	// Skips n samples
	blargg_err_t skip( int n );
	
	// Number of samples played/skipped since start_track()
	int sample_count() const                    { return out_time; }
	
	// True if track ended. Causes are end of source samples, end of fade,
	// or excessive silence.
	bool track_ended() const                    { return track_ended_; }
	
	// Clears state
	void stop();
	
// For use by callbacks

	// Sets internal "track ended" flag and stops generation of further source samples
	void set_track_ended()                      { emu_track_ended_ = true; }
	
	// For use by skip_() callback
	blargg_err_t skip_( int count );
	
// Implementation
public:
	Track_Filter();
	~Track_Filter();
	
private:
	callbacks_t* callbacks;
	setup_t setup_;
	const char* emu_error;
	bool silence_ignored_;
	
	// Timing
	int out_time;  // number of samples played since start of track
	int emu_time;  // number of samples emulator has generated since start of track
	int emu_track_ended_; // emulator has reached end of track
	volatile int track_ended_;
	void clear_time_vars();
	void end_track_if_error( blargg_err_t );
	
	// Fading
	int fade_start;
	int fade_step;
	bool is_fading() const;
	void handle_fade( sample_t out [], int count );
	
	// Silence detection
	int silence_time;   // absolute number of samples where most recent silence began
	int silence_count;  // number of samples of silence to play before using buf
	int buf_remain;     // number of samples left in silence buffer
	enum { buf_size = 2048 };
	blargg_vector<sample_t> buf;
	void fill_buf();
	void emu_play( sample_t out [], int count );
};

#endif
