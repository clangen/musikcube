// Linear downsampler with pre-low-pass

// $package
#ifndef DOWNSAMPLER_H
#define DOWNSAMPLER_H

#include "Resampler.h"

class Downsampler : public Resampler {
public:
	Downsampler();

protected:
	virtual blargg_err_t set_rate_( double );
	virtual void clear_();
	virtual sample_t const* resample_( sample_t**, sample_t const*, sample_t const [], int );

private:
	enum { stereo = 2 };
	enum { write_offset = 8 * stereo };
	int pos;
	int step;
};

#endif
