#pragma once

#include "mamedef.h"

#ifdef __cplusplus
extern "C" {
#endif

/* an interface for the OKIM6295 and similar chips */

/*
  Note about the playback frequency: the external clock is internally divided,
  depending on pin 7, by 132 (high) or 165 (low).
*/
/*typedef struct _okim6295_interface okim6295_interface;
struct _okim6295_interface
{
	int pin7;
};

extern const okim6295_interface okim6295_interface_pin7high;
extern const okim6295_interface okim6295_interface_pin7low;*/



//void okim6295_set_bank_base(running_device *device, int base);
//void okim6295_set_pin7(running_device *device, int pin7);

void okim6295_update(void *, stream_sample_t **outputs, int samples);
void * device_start_okim6295(int clock);
void device_stop_okim6295(void *);
void device_reset_okim6295(void *);

//READ8_DEVICE_HANDLER( okim6295_r );
//WRITE8_DEVICE_HANDLER( okim6295_w );
void okim6295_w(void *, offs_t offset, UINT8 data);

void okim6295_write_rom(void *, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
						const UINT8* ROMData);
void okim6295_set_mute_mask(void *, UINT32 MuteMask);


/*
    To help the various custom ADPCM generators out there,
    the following routines may be used.
*/
struct adpcm_state
{
	INT32	signal;
	INT32	step;
};
void reset_adpcm(struct adpcm_state *state);
INT16 clock_adpcm(struct adpcm_state *state, UINT8 nibble);

//DEVICE_GET_INFO( okim6295 );
//#define SOUND_OKIM6295 DEVICE_GET_INFO_NAME( okim6295 )

#ifdef __cplusplus
}
#endif
