// Band-limited sound synthesis buffer

// Blip_Buffer $vers
#ifndef BLIP_BUFFER_H
#define BLIP_BUFFER_H

#include "blargg_common.h"
#include "Blip_Buffer_impl.h"

typedef int blip_time_t;                    // Source clocks in current time frame
typedef BOOST::int16_t blip_sample_t;       // 16-bit signed output sample
int const blip_default_length = 1000 / 4;   // Default Blip_Buffer length (1/4 second)


//// Sample buffer for band-limited synthesis

class Blip_Buffer : public Blip_Buffer_ {
public:

	// Sets output sample rate and resizes and clears sample buffer
	blargg_err_t set_sample_rate( int samples_per_sec, int msec_length = blip_default_length );
	
	// Sets number of source time units per second
	void clock_rate( int clocks_per_sec );
	
	// Clears buffer and removes all samples
	void clear();
	
	// Use Blip_Synth to add waveform to buffer
	
	// Resamples to time t, then subtracts t from current time. Appends result of resampling
	// to buffer for reading.
	void end_frame( blip_time_t t );
	
	// Number of samples available for reading with read_samples()
	int samples_avail() const;
	
	// Reads at most n samples to out [0 to n-1] and returns number actually read. If stereo
	// is true, writes to out [0], out [2], out [4] etc. instead.
	int read_samples( blip_sample_t out [], int n, bool stereo = false );
	
// More features

	// Sets flag that tells some Multi_Buffer types that sound was added to buffer,
	// so they know that it needs to be mixed in. Only needs to be called once
	// per time frame that sound was added. Not needed if not using Multi_Buffer.
	void set_modified()                 { modified_ = true; }
	
	// Sets high-pass filter frequency, from 0 to 20000 Hz, where higher values reduce bass more
	void bass_freq( int frequency );

	int length() const;         // Length of buffer in milliseconds
	int sample_rate() const;    // Current output sample rate
	int clock_rate() const;     // Number of source time units per second
	int output_latency() const; // Number of samples delay from offset() to read_samples()
	
// Low-level features
	
	// Removes the first n samples
	void remove_samples( int n );
	
	// Returns number of clocks needed until n samples will be available.
	// If buffer cannot even hold n samples, returns number of clocks
	// until buffer becomes full.
	blip_time_t count_clocks( int n ) const;
	
	// Number of samples that should be mixed before calling end_frame( t )
	int count_samples( blip_time_t t ) const;
	
	// Mixes n samples into buffer
	void mix_samples( const blip_sample_t in [], int n );

// Resampled time (sorry, poor documentation right now)
	
	// Resampled time is fixed-point, in terms of output samples.
	
	// Converts clock count to resampled time
	blip_resampled_time_t resampled_duration( int t ) const         { return t * factor_; }
	
	// Converts clock time since beginning of current time frame to resampled time
	blip_resampled_time_t resampled_time( blip_time_t t ) const     { return t * factor_ + offset_; }
	
	// Returns factor that converts clock rate to resampled time
	blip_resampled_time_t clock_rate_factor( int clock_rate ) const;
	
// State save/load

	// Saves state, including high-pass filter and tails of last deltas.
	// All samples must have been read from buffer before calling this
	// (that is, samples_avail() must return 0).
	void save_state( blip_buffer_state_t* out );
	
	// Loads state. State must have been saved from Blip_Buffer with same
	// settings during same run of program; states can NOT be stored on disk.
	// Clears buffer before loading state.
	void load_state( const blip_buffer_state_t& in );

private:
	// noncopyable
	Blip_Buffer( const Blip_Buffer& );
	Blip_Buffer& operator = ( const Blip_Buffer& );

// Implementation
public:
	BLARGG_DISABLE_NOTHROW
	Blip_Buffer();
	~Blip_Buffer();
	void remove_silence( int n );
};


//// Adds amplitude changes to Blip_Buffer

template<int quality,int range> class Blip_Synth;

typedef Blip_Synth<8, 1> Blip_Synth_Fast; // faster, but less equalizer control
typedef Blip_Synth<12,1> Blip_Synth_Norm; // good for most things
typedef Blip_Synth<16,1> Blip_Synth_Good; // sharper filter cutoff

template<int quality,int range>
class Blip_Synth {
public:

	// Sets volume of amplitude delta unit
	void volume( double v )                     { impl.volume_unit( 1.0 / range * v ); }
	
	// Configures low-pass filter
	void treble_eq( const blip_eq_t& eq )       { impl.treble_eq( eq ); }
	
	// Gets/sets default Blip_Buffer
	Blip_Buffer* output() const                 { return impl.buf; }
	void output( Blip_Buffer* b )               { impl.buf = b; impl.last_amp = 0; }
	
	// Extends waveform to time t at current amplitude, then changes its amplitude to a
	// Using this requires a separate Blip_Synth for each waveform.
	void update( blip_time_t t, int a );

// Low-level interface
	
	// If no Blip_Buffer* is specified, uses one set by output() above
	
	// Adds amplitude transition at time t. Delta can be positive or negative.
	// The actual change in amplitude is delta * volume.
	void offset( blip_time_t t, int delta, Blip_Buffer* ) const;
	void offset( blip_time_t t, int delta               ) const { offset( t, delta, impl.buf ); }
	
	// Same as offset(), except code is inlined for higher performance
	void offset_inline( blip_time_t t, int delta, Blip_Buffer* buf ) const { offset_resampled( buf->to_fixed( t ), delta, buf ); }
	void offset_inline( blip_time_t t, int delta                   ) const { offset_resampled( impl.buf->to_fixed( t ), delta, impl.buf ); }
	
	// Works directly in terms of fractional output samples. Use resampled time functions in Blip_Buffer
	// to convert clock counts to resampled time.
	void offset_resampled( blip_resampled_time_t, int delta, Blip_Buffer* ) const;
	
// Implementation
public:
	BLARGG_DISABLE_NOTHROW

private:
#if BLIP_BUFFER_FAST
	Blip_Synth_Fast_ impl;
	typedef char coeff_t;
#else
	Blip_Synth_ impl;
	typedef short coeff_t;
	// Left halves of first difference of step response for each possible phase
	coeff_t phases [quality / 2 * blip_res];
public:
	Blip_Synth() : impl( phases, quality ) { }
#endif
};


//// Low-pass equalization parameters

class blip_eq_t {
	double treble, kaiser;
	int rolloff_freq, sample_rate, cutoff_freq;
public:
	// Logarithmic rolloff to treble dB at half sampling rate. Negative values reduce
	// treble, small positive values (0 to 5.0) increase treble.
	blip_eq_t( double treble_db = 0 );
	
	// See blip_buffer.txt
	blip_eq_t( double treble, int rolloff_freq, int sample_rate, int cutoff_freq = 0,
			double kaiser = 5.2 );
	
	// Generate center point and right half of impulse response
	virtual void generate( float out [], int count ) const;
	virtual ~blip_eq_t() { }
	
	enum { oversample = blip_res };
	static int calc_count( int quality ) { return (quality - 1) * (oversample / 2) + 1; }
};

#include "Blip_Buffer_impl2.h"

#endif
