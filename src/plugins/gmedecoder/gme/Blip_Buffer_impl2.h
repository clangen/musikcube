// Internal stuff here to keep public header uncluttered

// Blip_Buffer $vers
#ifndef BLIP_BUFFER_IMPL2_H
#define BLIP_BUFFER_IMPL2_H

//// Compatibility

BLARGG_DEPRECATED( int const blip_low_quality  =  8; )
BLARGG_DEPRECATED( int const blip_med_quality  =  8; )
BLARGG_DEPRECATED( int const blip_good_quality = 12; )
BLARGG_DEPRECATED( int const blip_high_quality = 16; )

BLARGG_DEPRECATED( int const blip_sample_max = 32767; )

// Number of bits in raw sample that covers normal output range. Less than 32 bits to give
// extra amplitude range. That is,
// +1 << (blip_sample_bits-1) = +1.0
// -1 << (blip_sample_bits-1) = -1.0
int const blip_sample_bits = 30;

//// BLIP_READER_

//// Optimized reading from Blip_Buffer, for use in custom sample buffer or mixer

// Begins reading from buffer. Name should be unique to the current {} block.
#define BLIP_READER_BEGIN( name, blip_buffer ) \
	const Blip_Buffer::delta_t* BLARGG_RESTRICT name##_reader_buf = (blip_buffer).read_pos();\
	int name##_reader_accum = (blip_buffer).integrator()

// Gets value to pass to BLIP_READER_NEXT()
#define BLIP_READER_BASS( blip_buffer ) (blip_buffer).highpass_shift()

// Constant value to use instead of BLIP_READER_BASS(), for slightly more optimal
// code at the cost of having no bass_freq() functionality
int const blip_reader_default_bass = 9;

// Current sample as 16-bit signed value
#define BLIP_READER_READ( name )        (name##_reader_accum >> (blip_sample_bits - 16))

// Current raw sample in full internal resolution
#define BLIP_READER_READ_RAW( name )    (name##_reader_accum)

// Advances to next sample
#define BLIP_READER_NEXT( name, bass ) \
	(void) (name##_reader_accum += *name##_reader_buf++ - (name##_reader_accum >> (bass)))

// Ends reading samples from buffer. The number of samples read must now be removed
// using Blip_Buffer::remove_samples().
#define BLIP_READER_END( name, blip_buffer ) \
	(void) ((blip_buffer).set_integrator( name##_reader_accum ))

#define BLIP_READER_ADJ_( name, offset ) (name##_reader_buf += offset)

int const blip_reader_idx_factor = sizeof (Blip_Buffer::delta_t);

#define BLIP_READER_NEXT_IDX_( name, bass, idx ) {\
	name##_reader_accum -= name##_reader_accum >> (bass);\
	name##_reader_accum += name##_reader_buf [(idx)];\
}

#define BLIP_READER_NEXT_RAW_IDX_( name, bass, idx ) {\
	name##_reader_accum -= name##_reader_accum >> (bass);\
	name##_reader_accum +=\
			*(Blip_Buffer::delta_t const*) ((char const*) name##_reader_buf + (idx));\
}

//// BLIP_CLAMP

#if defined (_M_IX86) || defined (_M_IA64) || defined (__i486__) || \
		defined (__x86_64__) || defined (__ia64__) || defined (__i386__)
	#define BLIP_X86 1
	#define BLIP_CLAMP_( in ) in < -0x8000 || 0x7FFF < in
#else
	#define BLIP_CLAMP_( in ) (blip_sample_t) in != in
#endif

// Clamp sample to blip_sample_t range
#define BLIP_CLAMP( sample, out )\
	{ if ( BLIP_CLAMP_( (sample) ) ) (out) = ((sample) >> 31) ^ 0x7FFF; }


//// Blip_Synth

// (in >> sh & mask) * mul
#define BLIP_SH_AND_MUL( in, sh, mask, mul ) \
((int) (in) / ((1U << (sh)) / (mul)) & (unsigned) ((mask) * (mul)))

// (T*) ptr + (off >> sh)
#define BLIP_PTR_OFF_SH( T, ptr, off, sh ) \
	((T*) (BLIP_SH_AND_MUL( off, sh, -1, sizeof (T) ) + (char*) (ptr)))

template<int quality,int range>
inline void Blip_Synth<quality,range>::offset_resampled( blip_resampled_time_t time,
		int delta, Blip_Buffer* blip_buf ) const
{
#if BLIP_BUFFER_FAST
	int const half_width = 1;
#else
	int const half_width = quality / 2;
#endif
	
	Blip_Buffer::delta_t* BLARGG_RESTRICT buf = blip_buf->delta_at( time );
	
	delta *= impl.delta_factor;

	int const phase_shift = BLIP_BUFFER_ACCURACY - BLIP_PHASE_BITS;
	int const phase = (half_width & (half_width - 1)) ?
		(int) BLIP_SH_AND_MUL( time, phase_shift, blip_res - 1, sizeof (coeff_t) ) * half_width :
		(int) BLIP_SH_AND_MUL( time, phase_shift, blip_res - 1, sizeof (coeff_t) * half_width );
	
#if BLIP_BUFFER_FAST
	int left = buf [0] + delta;
	
	// Kind of crappy, but doing shift after multiply results in overflow.
	// Alternate way of delaying multiply by delta_factor results in worse
	// sub-sample resolution.
	int right = (delta >> BLIP_PHASE_BITS) * phase;
	#if BLIP_BUFFER_NOINTERP
		// TODO: remove? (just a hack to see how it sounds)
		right = 0;
	#endif
	left  -= right;
	right += buf [1];
	
	buf [0] = left;
	buf [1] = right;
#else
	
	int const fwd = -quality / 2;
	int const rev = fwd + quality - 2;
	
	coeff_t const* BLARGG_RESTRICT imp = (coeff_t const*) ((char const*) phases + phase);
	int const phase2 = phase + phase - (blip_res - 1) * half_width * sizeof (coeff_t);
	
	#define BLIP_MID_IMP imp = (coeff_t const*) ((char const*) imp - phase2);
	
	#if BLIP_MAX_QUALITY > 16
		// General version for any quality
		if ( quality != 8 && quality != 12 && quality != 16 )
		{
			buf += fwd;
			
			// left half
			for ( int n = half_width / 2; --n >= 0; )
			{
				buf [0] += imp [0] * delta;
				buf [1] += imp [1] * delta;
				imp += 2;
				buf += 2;
			}
			
			// mirrored right half
			BLIP_MID_IMP
			for ( int n = half_width / 2; --n >= 0; )
			{
				buf [0] +=   imp  [-1] * delta;
				buf [1] += *(imp -= 2) * delta;
				buf += 2;
			}
			
			return;
		}
	#endif
	
	// Unrolled versions for qualities 8, 12, and 16
	
	#if BLIP_X86
		// This gives better code for x86
		#define BLIP_ADD( out, in ) \
			buf [out] += imp [in] * delta
		
		#define BLIP_FWD( i ) {\
			BLIP_ADD( fwd     + i, i     );\
			BLIP_ADD( fwd + 1 + i, i + 1 );\
		}
		
		#define BLIP_REV( r ) {\
			BLIP_ADD( rev     - r, r + 1 );\
			BLIP_ADD( rev + 1 - r, r     );\
		}
		
		BLIP_FWD( 0 )
		BLIP_FWD( 2 )
		if ( quality > 8  ) BLIP_FWD( 4 )
		if ( quality > 12 ) BLIP_FWD( 6 )
			BLIP_MID_IMP
		if ( quality > 12 ) BLIP_REV( 6 )
		if ( quality > 8  ) BLIP_REV( 4 )
		BLIP_REV( 2 )
		BLIP_REV( 0 )
	
	#else
		// Help RISC processors and simplistic compilers by reading ahead of writes
		#define BLIP_FWD( i ) {\
			int t0 =          i0 * delta + buf [fwd     + i];\
			int t1 = imp [i + 1] * delta + buf [fwd + 1 + i];\
			i0 =           imp [i + 2];\
			buf [fwd     + i] = t0;\
			buf [fwd + 1 + i] = t1;\
		}
		
		#define BLIP_REV( r ) {\
			int t0 =      i0 * delta + buf [rev     - r];\
			int t1 = imp [r] * delta + buf [rev + 1 - r];\
			i0 =           imp [r - 1];\
			buf [rev     - r] = t0;\
			buf [rev + 1 - r] = t1;\
		}
		
		int i0 = *imp;
		BLIP_FWD( 0 )
		if ( quality > 8  ) BLIP_FWD( 2 )
		if ( quality > 12 ) BLIP_FWD( 4 )
		{
			int const mid = half_width - 1;
			int t0 =        i0 * delta + buf [fwd + mid - 1];
			int t1 = imp [mid] * delta + buf [fwd + mid    ];
			BLIP_MID_IMP
			i0           = imp [mid];
			buf [fwd + mid - 1] = t0;
			buf [fwd + mid    ] = t1;
		}
		if ( quality > 12 ) BLIP_REV( 6 )
		if ( quality > 8  ) BLIP_REV( 4 )
		BLIP_REV( 2 )
		
		int t0 =   i0 * delta + buf [rev    ];
		int t1 = *imp * delta + buf [rev + 1];
		buf [rev    ] = t0;
		buf [rev + 1] = t1;
	#endif
	
#endif
}

template<int quality,int range>
#if BLIP_BUFFER_FAST
	inline
#endif
void Blip_Synth<quality,range>::offset( blip_time_t t, int delta, Blip_Buffer* buf ) const
{
	offset_resampled( buf->to_fixed( t ), delta, buf );
}

template<int quality,int range>
#if BLIP_BUFFER_FAST
	inline
#endif
void Blip_Synth<quality,range>::update( blip_time_t t, int amp )
{
	int delta = amp - impl.last_amp;
	impl.last_amp = amp;
	offset_resampled( impl.buf->to_fixed( t ), delta, impl.buf );
}


//// blip_eq_t

inline blip_eq_t::blip_eq_t( double t ) :
		treble( t ), kaiser( 5.2 ), rolloff_freq( 0 ), sample_rate( 44100 ), cutoff_freq( 0 ) { }
inline blip_eq_t::blip_eq_t( double t, int rf, int sr, int cf, double k ) :
		treble( t ), kaiser( k ), rolloff_freq( rf ), sample_rate( sr ), cutoff_freq( cf ) { }


//// Blip_Buffer

inline int  Blip_Buffer::length() const         { return length_; }
inline int  Blip_Buffer::samples_avail() const  { return (int) (offset_ >> BLIP_BUFFER_ACCURACY); }
inline int  Blip_Buffer::sample_rate() const    { return sample_rate_; }
inline int  Blip_Buffer::output_latency() const { return BLIP_MAX_QUALITY / 2; }
inline int  Blip_Buffer::clock_rate() const     { return clock_rate_; }
inline void Blip_Buffer::clock_rate( int cps )  { factor_ = clock_rate_factor( clock_rate_ = cps ); }

inline void Blip_Buffer::remove_silence( int count )
{
	// fails if you try to remove more samples than available
	assert( count <= samples_avail() );
	offset_ -= (blip_resampled_time_t) count << BLIP_BUFFER_ACCURACY;
}

#endif
