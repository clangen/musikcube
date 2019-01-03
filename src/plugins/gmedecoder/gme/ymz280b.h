/**********************************************************************************************
 *
 *   Yamaha YMZ280B driver
 *   by Aaron Giles
 *
 **********************************************************************************************/

#pragma once

#include "mamedef.h"

//#include "devcb.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _ymz280b_interface ymz280b_interface;
struct _ymz280b_interface
{
	//void (*irq_callback)(const device_config *device, int state);	/* irq callback */
	void (*irq_callback)(int state);	/* irq callback */
	//devcb_read8 ext_read;			/* external RAM read */
	//devcb_write8 ext_write;		/* external RAM write */
};

/*READ8_DEVICE_HANDLER ( ymz280b_r );
WRITE8_DEVICE_HANDLER( ymz280b_w );

DEVICE_GET_INFO( ymz280b );
#define SOUND_YMZ280B DEVICE_GET_INFO_NAME( ymz280b )*/

void ymz280b_update(void *, stream_sample_t **outputs, int samples);
void * device_start_ymz280b(int clock);
void device_stop_ymz280b(void *);
void device_reset_ymz280b(void *);

UINT8 ymz280b_r(void *, offs_t offset);
void ymz280b_w(void *, offs_t offset, UINT8 data);
void ymz280b_write_rom(void *, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					   const UINT8* ROMData);

void ymz280b_set_mute_mask(void *, UINT32 MuteMask);

#ifdef __cplusplus
}
#endif
