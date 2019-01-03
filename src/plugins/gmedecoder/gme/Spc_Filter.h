// Simple low-pass and high-pass filter to better match sound output of a SNES

// snes_spc $vers
#ifndef SPC_FILTER_H
#define SPC_FILTER_H

#include "blargg_common.h"

struct Spc_Filter {
public:
	
	// Filters count samples of stereo sound in place. Count must be a multiple of 2.
	typedef short sample_t;
	void run( sample_t io [], int count );
	
// Optional features

	// Clears filter to silence
	void clear();
	
	// Sets gain (volume), where gain_unit is normal. Gains greater than gain_unit
	// are fine, since output is clamped to 16-bit sample range.
	enum { gain_unit = 0x100 };
	void set_gain( int gain );
	
	// Enables/disables filtering (when disabled, gain is still applied)
	void enable( bool b );
	
	// Sets amount of bass (logarithmic scale)
	enum { bass_none =  0 };
	enum { bass_norm =  8 }; // normal amount
	enum { bass_max  = 31 };
	void set_bass( int bass );
	
public:
	Spc_Filter();
	BLARGG_DISABLE_NOTHROW
private:
	enum { gain_bits = 8 };
	int gain;
	int bass;
	bool enabled;
    bool limiting;
	struct chan_t { int p1, pp1, sum; };
	chan_t ch [2];
    short hard_limit_table[131072];
    void build_limit_table();
    inline short limit_sample(int sample);
};

inline void Spc_Filter::enable( bool b )  { enabled = b; }

inline void Spc_Filter::set_gain( int g ) { gain = g; }

inline void Spc_Filter::set_bass( int b ) { bass = b; }

#endif
