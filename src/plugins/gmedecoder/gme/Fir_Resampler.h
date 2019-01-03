// Finite impulse response (FIR) resampler with adjustable FIR size

// $package
#ifndef FIR_RESAMPLER_H
#define FIR_RESAMPLER_H

#include "Resampler.h"

template<int width>
class Fir_Resampler;

// Use one of these typedefs
typedef Fir_Resampler< 8> Fir_Resampler_Fast;
typedef Fir_Resampler<16> Fir_Resampler_Norm;
typedef Fir_Resampler<24> Fir_Resampler_Good;

// Implementation
class Fir_Resampler_ : public Resampler {
protected:
	virtual blargg_err_t set_rate_( double );
	virtual void clear_();

protected:
	enum { stereo = 2 };
	enum { max_res = 32 }; // TODO: eliminate and keep impulses on freestore?
	sample_t const* imp;
	int const width_;
	sample_t* impulses;
	
	Fir_Resampler_( int width, sample_t [] );
};

// Width is number of points in FIR. More points give better quality and
// rolloff effectiveness, and take longer to calculate.
template<int width>
class Fir_Resampler : public Fir_Resampler_ {
	enum { min_width = (width < 4 ? 4 : width) };
	enum { adj_width = min_width / 4 * 4 + 2 };
	enum { write_offset = adj_width * stereo };
	short impulses [max_res * (adj_width + 2)];
public:
	Fir_Resampler() : Fir_Resampler_( adj_width, impulses ) { }

protected:
	virtual sample_t const* resample_( sample_t**, sample_t const*, sample_t const [], int );
};

template<int width>
Resampler::sample_t const* Fir_Resampler<width>::resample_( sample_t** out_,
		sample_t const* out_end, sample_t const in [], int in_size )
{
	in_size -= write_offset;
	if ( in_size > 0 )
	{
		sample_t* BLARGG_RESTRICT out = *out_;
		sample_t const* const in_end = in + in_size;
		sample_t const* imp = this->imp;
		
		do
		{
			// accumulate in extended precision
			int pt = imp [0];
			int l = pt * in [0];
			int r = pt * in [1];
			if ( out >= out_end )
				break;
			for ( int n = (adj_width - 2) / 2; n; --n )
			{
				pt = imp [1];
				l += pt * in [2];
				r += pt * in [3];
				
				// pre-increment more efficient on some RISC processors
				imp += 2;
				pt = imp [0];
				r += pt * in [5];
				in += 4;
				l += pt * in [0];
			}
			pt = imp [1];
			l += pt * in [2];
			r += pt * in [3];
			
			// these two "samples" after the end of the impulse give the
			// proper offsets to the next input sample and next impulse
			in  = (sample_t const*) ((char const*) in  + imp [2]); // some negative value
			imp = (sample_t const*) ((char const*) imp + imp [3]); // small positive or large negative
			
			out [0] = sample_t (l >> 15);
			out [1] = sample_t (r >> 15);
			out += 2;
		}
		while ( in < in_end );
		
		this->imp = imp;
		*out_ = out;
	}
	return in;
}

#endif
