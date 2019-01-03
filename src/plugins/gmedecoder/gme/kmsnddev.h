/* libnezp by Mamiya */

#ifndef KMSNDDEV_H__
#define KMSNDDEV_H__
#ifdef __cplusplus
extern "C" {
#endif

#include "nestypes.h"

typedef struct KMIF_SOUND_DEVICE {
	void *ctx;
	void (*release)(void *ctx);
	void (*reset)(void *ctx, Uint32 clock, Uint32 freq);
	int (*synth)(void *ctx);
	void (*volume)(void *ctx, Int32 v);
	void (*write)(void *ctx, Uint32 a, Uint32 v);
	Uint32 (*read)(void *ctx, Uint32 a);
	void (*setinst)(void *ctx, Uint32 n, void *p, Uint32 l);
#if 0
	void (*setrate)(void *ctx, Uint32 clock, Uint32 freq);
	void (*getinfo)(void *ctx, KMCH_INFO *cip, );
	void (*volume2)(void *ctx, Uint8 *volp, Uint32 numch);
	/* 0x00(mute),0x70(x1/2),0x80(x1),0x90(x2) */
#endif
} KMIF_SOUND_DEVICE;

#ifdef __cplusplus
}
#endif
#endif /* KMSNDDEV_H__ */
