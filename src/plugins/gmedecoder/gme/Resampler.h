// Common interface for resamplers

// $package
#ifndef RESAMPLER_H
#define RESAMPLER_H

#include "blargg_common.h"

class Resampler {
public:
	
	virtual ~Resampler();
	
	// Sets input/output resampling ratio
	blargg_err_t set_rate( double );
	
	// Current input/output ratio
	double rate() const             { return rate_; }
	
	// Samples are 16-bit signed
	typedef short sample_t;

// One of two different buffering schemes can be used, as decided by the caller:

// External buffering (caller provides input buffer)
	
	// Resamples in to at most n out samples and returns number of samples actually
	// written. Sets *in_size to number of input samples that aren't needed anymore
	// and should be removed from input.
	int resample( sample_t out [], int n, sample_t const in [], int* in_size );

// Internal buffering (resampler manages buffer)
	
	// Resizes input buffer to n samples, then clears it
	blargg_err_t resize_buffer( int n );
	
	// Clears input buffer
	void clear();
	
	// Writes at most n samples to input buffer and returns number actually written.
	// Result will be less than n if there isn't enough free space in buffer.
	int write( sample_t const in [], int n );
	
	// Number of input samples in buffer
	int written() const             { return write_pos; }
	
	// Removes first n input samples from buffer, fewer if there aren't that many.
	// Returns number of samples actually removed.
	int skip_input( int n );
	
	// Resamples input to at most n output samples. Returns number of samples
	// actually written to out. Result will be less than n if there aren't
	// enough input samples in buffer.
	int read( sample_t out [], int n );

// Direct writing to input buffer, instead of using write( in, n ) above

	// Pointer to place to write input samples
	sample_t* buffer()              { return &buf [write_pos]; }
	
	// Number of samples that can be written to buffer()
	int buffer_free() const         { return buf.size() - write_pos; }
	
	// Notifies resampler that n input samples have been written to buffer().
	// N must not be greater than buffer_free().
	void write( int n );

// Derived interface
protected:
	virtual blargg_err_t set_rate_( double rate ) BLARGG_PURE( ; )
	
	virtual void clear_() { }
	
	// Resample as many available in samples as will fit within out_size and
	// return pointer past last input sample read and set *out just past
	// the last output sample.
	virtual sample_t const* resample_( sample_t** out, sample_t const* out_end,
			sample_t const in [], int in_size ) BLARGG_PURE( { return in; } )

// Implementation
public:
	Resampler();

private:
	blargg_vector<sample_t> buf;
	int write_pos;
	double rate_;
	
	int resample_wrapper( sample_t out [], int* out_size,
			sample_t const in [], int in_size );
};

inline void Resampler::write( int count )
{
	write_pos += count;
	assert( (unsigned) write_pos <= buf.size() );
}

inline blargg_err_t Resampler::set_rate_( double r )
{
	rate_ = r;
	return blargg_ok;
}

inline blargg_err_t Resampler::set_rate( double r )
{
	return set_rate_( r );
}

#endif
