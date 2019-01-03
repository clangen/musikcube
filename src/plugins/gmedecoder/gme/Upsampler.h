// Increases sampling rate using linear interpolation

// $package
#ifndef UPSAMPLER_H
#define UPSAMPLER_H

#include "Resampler.h"

class Upsampler : public Resampler {
public:
	Upsampler();
	
protected:
	virtual blargg_err_t set_rate_( double );
	virtual void clear_();
	virtual sample_t const* resample_( sample_t**, sample_t const*, sample_t const [], int );

protected:
	enum { stereo = 2 };
	enum { write_offset = 2 * stereo };
	int pos;
	int step;
};

#endif
