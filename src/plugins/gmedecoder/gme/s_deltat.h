#ifndef S_DELTAT_H__
#define S_DELTAT_H__

#include "kmsnddev.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
	/* MSX-AUDIO   */ YMDELTATPCM_TYPE_Y8950,
	/* OPNA ADPCM  */ YMDELTATPCM_TYPE_YM2608,
	/* OPNB ADPCMB */ YMDELTATPCM_TYPE_YM2610
};

KMIF_SOUND_DEVICE *YMDELTATPCMSoundAlloc(Uint32 ymdeltatpcm_type);

#ifdef __cplusplus
}
#endif

#endif /* S_DELTAT_H__ */

