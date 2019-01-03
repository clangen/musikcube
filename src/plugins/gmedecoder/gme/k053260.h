/*********************************************************

    Konami 053260 PCM/ADPCM Sound Chip

*********************************************************/

#pragma once

#include "mamedef.h"

#ifdef __cplusplus
extern "C" {
#endif

//#include "devlegcy.h"

/*typedef struct _k053260_interface k053260_interface;
struct _k053260_interface {
	const char *rgnoverride;
	timer_expired_func irq;			// called on SH1 complete cycle ( clock / 32 ) //
};*/


void k053260_update(void *, stream_sample_t **outputs, int samples);
void * device_start_k053260(int clock);
void device_stop_k053260(void *);
void device_reset_k053260(void *);

//WRITE8_DEVICE_HANDLER( k053260_w );
//READ8_DEVICE_HANDLER( k053260_r );
void k053260_w(void *, offs_t offset, UINT8 data);
UINT8 k053260_r(void *, offs_t offset);

void k053260_write_rom(void *, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);
void k053260_set_mute_mask(void *, UINT32 MuteMask);

//DECLARE_LEGACY_SOUND_DEVICE(K053260, k053260);

#ifdef __cplusplus
};
#endif
