// Common interface to game music file emulators

// Game_Music_Emu $vers
#ifndef MUSIC_EMU_H
#define MUSIC_EMU_H

#include "Gme_File.h"
#include "Track_Filter.h"
#include "blargg_errors.h"
class Multi_Buffer;

struct gme_t : public Gme_File, private Track_Filter::callbacks_t {
public:
	// Sets output sample rate. Must be called only once before loading file.
	blargg_err_t set_sample_rate( int sample_rate );

	// Sample rate sound is generated at
	int sample_rate() const;
	
// File loading

	// See Gme_Loader.h

// Basic playback

	// Starts a track, where 0 is the first track. Also clears warning string.
	blargg_err_t start_track( int );
	
	// Generates 'count' samples info 'buf'. Output is in stereo. Any emulation
	// errors set warning string, and major errors also end track.
	typedef short sample_t;
	blargg_err_t play( int count, sample_t* buf );
	
// Track information
	
	// See Gme_File.h
	
	// Index of current track or -1 if one hasn't been started
	int current_track() const;

	// Info for currently playing track
	using Gme_File::track_info;
	blargg_err_t track_info( track_info_t* out ) const;
    blargg_err_t set_track_info( const track_info_t* in );
    blargg_err_t set_track_info( const track_info_t* in, int track_number );

	struct Hash_Function
	{
		virtual void hash_( byte const* data, size_t size ) BLARGG_PURE( ; )
	};
	virtual blargg_err_t hash_( Hash_Function& ) const BLARGG_PURE( ; )
    
    blargg_err_t save( gme_writer_t writer, void* your_data) const;

// Track status/control

	// Number of milliseconds played since beginning of track (1000 per second)
	int tell() const;
	
	// Seeks to new time in track. Seeking backwards or far forward can take a while.
	blargg_err_t seek( int msec );
	
	// Skips n samples
	blargg_err_t skip( int n );
	
	// True if a track has reached its end
	bool track_ended() const;
	
	// Sets start time and length of track fade out. Once fade ends track_ended() returns
	// true. Fade time must be set after track has been started, and can be changed
	// at any time.
	void set_fade( int start_msec, int length_msec = 8000 );
	
	// Disables automatic end-of-track detection and skipping of silence at beginning
	void ignore_silence( bool disable = true );
	
// Voices

	// Number of voices used by currently loaded file
	int voice_count() const;
	
	// Name of voice i, from 0 to voice_count()-1
	const char* voice_name( int i ) const;
	
	// Mutes/unmutes voice i, where voice 0 is first voice
	void mute_voice( int index, bool mute = true );
	
	// Sets muting state of all voices at once using a bit mask, where -1 mutes them all,
	// 0 unmutes them all, 0x01 mutes just the first voice, etc.
	void mute_voices( int mask );

// Sound customization
	
	// Adjusts song tempo, where 1.0 = normal, 0.5 = half speed, 2.0 = double speed.
	// Track length as returned by track_info() assumes a tempo of 1.0.
	void set_tempo( double );
	
	// Changes overall output amplitude, where 1.0 results in minimal clamping.
	// Must be called before set_sample_rate().
	void set_gain( double );
	
	// Requests use of custom multichannel buffer. Only supported by "classic" emulators;
	// on others this has no effect. Should be called only once *before* set_sample_rate().
	virtual void set_buffer( class Multi_Buffer* ) { }
	
// Sound equalization (treble/bass)

	// Frequency equalizer parameters (see gme.txt)
	// See gme.h for definition of struct gme_equalizer_t.
	typedef gme_equalizer_t equalizer_t;
	
	// Current frequency equalizater parameters
	equalizer_t const& equalizer() const;
	
	// Sets frequency equalizer parameters
	void set_equalizer( equalizer_t const& );
	
	// Equalizer preset for a TV speaker
	static equalizer_t const tv_eq;
	
// Derived interface
protected:
	// Cause any further generated samples to be silence, instead of calling play_()
	void set_track_ended()                      { track_filter.set_track_ended(); }
	
	// If more than secs of silence are encountered, track is ended
	void set_max_initial_silence( int secs )    { tfilter.max_initial = secs; }
	
	// Sets rate emulator is run at when scanning ahead for silence. 1=100%, 2=200% etc.
	void set_silence_lookahead( int rate )      { tfilter.lookahead = rate; }
	
	// Sets number of voices
	void set_voice_count( int n )               { voice_count_ = n; }
	
	// Sets names of voices
	void set_voice_names( const char* const names [] );
	
	// Current gain
	double gain() const                         { return gain_; }
	
	// Current tempo
	double tempo() const                        { return tempo_; }
	
	// Re-applies muting mask using mute_voices_()
	void remute_voices();
	
// Overrides should do the indicated task
	
	// Set sample rate as close as possible to sample_rate, then call
	// Music_Emu::set_sample_rate_() with the actual rate used.
	virtual blargg_err_t set_sample_rate_( int sample_rate )    BLARGG_PURE( ; )
	
	// Set equalizer parameters
	virtual void set_equalizer_( equalizer_t const& )           { }
	
	// Mute voices based on mask
	virtual void mute_voices_( int mask )                       BLARGG_PURE( ; )
	
	// Set tempo to t, which is constrained to the range 0.02 to 4.0.
	virtual void set_tempo_( double t )                         BLARGG_PURE( ; )
	
	// Start track t, where 0 is the first track
	virtual blargg_err_t start_track_( int t )                  BLARGG_PURE( ; ) // tempo is set before this
	
	// Generate count samples into *out. Count will always be even.
	virtual blargg_err_t play_( int count, sample_t out [] )    BLARGG_PURE( ; )
	
	// Skip count samples. Count will always be even.
	virtual blargg_err_t skip_( int count );

    // Save current state of file to specified writer.
    virtual blargg_err_t save_( gme_writer_t, void* ) const { return "Not supported by this format"; }

    // Set track info
    virtual blargg_err_t set_track_info_( const track_info_t*, int ) { return "Not supported by this format"; }
    
// Implementation
public:
	gme_t();
	~gme_t();
	BLARGG_DEPRECATED( const char** voice_names() const { return CONST_CAST(const char**,voice_names_); } )

protected:
	virtual void unload();
	virtual void pre_load();
	virtual blargg_err_t post_load();

private:
	Track_Filter::setup_t tfilter;
	Track_Filter track_filter;
	equalizer_t equalizer_;
	const char* const* voice_names_;
	int voice_count_;
	int mute_mask_;
	double tempo_;
	double gain_;
	int sample_rate_;
	int current_track_;
    
    bool fade_set;
    int length_msec;
    int fade_msec;
	
	void clear_track_vars();
	int msec_to_samples( int msec ) const;
	
	friend Music_Emu* gme_new_emu( gme_type_t, int );
	friend void gme_effects( Music_Emu const*, gme_effects_t* );
	friend void gme_set_effects( Music_Emu*, gme_effects_t const* );
	friend void gme_set_stereo_depth( Music_Emu*, double );
	friend const char** gme_voice_names ( Music_Emu const* );
	
protected:
	Multi_Buffer* effects_buffer_;
};

// base class for info-only derivations
struct Gme_Info_ : Music_Emu
{
	virtual blargg_err_t set_sample_rate_( int sample_rate );
	virtual void set_equalizer_( equalizer_t const& );
	virtual void mute_voices_( int mask );
	virtual void set_tempo_( double );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t play_( int count, sample_t out [] );
	virtual void pre_load();
	virtual blargg_err_t post_load();
};

inline blargg_err_t Music_Emu::track_info( track_info_t* out ) const
{
	return track_info( out, current_track_ );
}

inline blargg_err_t Music_Emu::save(gme_writer_t writer, void *your_data) const
{
    return save_( writer, your_data );
}

inline blargg_err_t Music_Emu::set_track_info(const track_info_t *in)
{
    return set_track_info_( in, current_track_ );
}

inline blargg_err_t Music_Emu::set_track_info(const track_info_t *in, int track)
{
    return set_track_info_( in, track );
}

inline int Music_Emu::sample_rate() const           { return sample_rate_; }
inline int Music_Emu::voice_count() const           { return voice_count_; }
inline int Music_Emu::current_track() const         { return current_track_; }
inline bool Music_Emu::track_ended() const          { return track_filter.track_ended(); }
inline const Music_Emu::equalizer_t& Music_Emu::equalizer() const { return equalizer_; }

inline void Music_Emu::ignore_silence( bool b )     { track_filter.ignore_silence( b ); }
inline void Music_Emu::set_tempo_( double t )       { tempo_ = t; }
inline void Music_Emu::remute_voices()              { mute_voices( mute_mask_ ); }

inline void Music_Emu::set_voice_names( const char* const p [] ) { voice_names_ = p; }

inline void Music_Emu::mute_voices_( int ) { }

inline void Music_Emu::set_gain( double g )
{
	assert( !sample_rate() ); // you must set gain before setting sample rate
	gain_ = g;
}

inline blargg_err_t Music_Emu::start_track_( int )  { return blargg_ok; }

inline blargg_err_t Music_Emu::set_sample_rate_( int ) { return blargg_ok; }

inline blargg_err_t Music_Emu::play_( int, sample_t [] ) { return blargg_ok; }

inline blargg_err_t Music_Emu::hash_( Hash_Function& ) const { return BLARGG_ERR( BLARGG_ERR_CALLER, "no hashing function defined" ); }

inline void Music_Emu::Hash_Function::hash_( byte const*, size_t ) { }

#endif
