#pragma once

#include "mamedef.h"

#ifdef __cplusplus
extern "C" {
#endif

//#ifndef __K051649_H__
//#define __K051649_H__

//#include "devlegcy.h"

/*WRITE8_DEVICE_HANDLER( k051649_waveform_w );
READ8_DEVICE_HANDLER( k051649_waveform_r );
WRITE8_DEVICE_HANDLER( k051649_volume_w );
WRITE8_DEVICE_HANDLER( k051649_frequency_w );
WRITE8_DEVICE_HANDLER( k051649_keyonoff_w );

WRITE8_DEVICE_HANDLER( k052539_waveform_w );

DECLARE_LEGACY_SOUND_DEVICE(K051649, k051649);*/

void k051649_update(void *, stream_sample_t **outputs, int samples);
void * device_start_k051649(int clock);
void device_stop_k051649(void *);
void device_reset_k051649(void *);

void k051649_waveform_w(void *, offs_t offset, UINT8 data);
UINT8 k051649_waveform_r(void *, offs_t offset);
void k051649_volume_w(void *, offs_t offset, UINT8 data);
void k051649_frequency_w(void *, offs_t offset, UINT8 data);
void k051649_keyonoff_w(void *, offs_t offset, UINT8 data);

void k052539_waveform_w(void *, offs_t offset, UINT8 data);

void k051649_w(void *, offs_t offset, UINT8 data);

void k051649_set_mute_mask(void *, UINT32 MuteMask);

//#endif /* __K051649_H__ */

#ifdef __cplusplus
};
#endif
