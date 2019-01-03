// Library configuration. Modify this file as necessary.

// $package
#ifndef BLARGG_CONFIG_H
#define BLARGG_CONFIG_H

// Uncomment a #define line below to have effect described.

// Allow static linking with this library and one of my other libraries
// in the same program.
//#define BLARGG_NAMESPACE blargg_gme

// Use zlib for transparent decompression of gzipped files.
//#define HAVE_ZLIB_H

// Support only listed music types. Remove a line to disable that type.
/* #define GME_TYPE_LIST \
	gme_ay_type,\
	gme_gbs_type,\
	gme_gym_type,\
	gme_hes_type,\
	gme_kss_type,\
	gme_nsf_type,\
	gme_nsfe_type,\
	gme_sap_type,\
    gme_sfm_type,\
	gme_sgc_type,\
	gme_spc_type,\
	gme_vgm_type,\
	gme_vgz_type
*/

// Enable platform-specific optimizations.
//#define BLARGG_NONPORTABLE 1

// Use faster sample rate convertor for SPC music.
//#define GME_SPC_FAST_RESAMPLER 1

// Use faster sample rate convertor for VGM and GYM music.
//#define GME_VGM_FAST_RESAMPLER 1

// Use faster, significantly lower quality sound synthesis for classic emulators.
//#define BLIP_BUFFER_FAST 1

// Reduce memory usage of gme.h by disabling gme_set_effects_config().
//#define GME_DISABLE_EFFECTS 1

// Force library to use assume big-endian processor.
//#define BLARGG_BIG_ENDIAN 1

// Use standard config.h if present
#ifdef HAVE_CONFIG_H
	#include "config.h"
#endif

#endif
