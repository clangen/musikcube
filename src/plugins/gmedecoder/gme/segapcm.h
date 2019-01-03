/*********************************************************/
/*    SEGA 8bit PCM                                      */
/*********************************************************/

#pragma once

#include "mamedef.h"

#define   BANK_256    (11)
#define   BANK_512    (12)
#define   BANK_12M    (13)
#define   BANK_MASK7    (0x70<<16)
#define   BANK_MASKF    (0xf0<<16)
#define   BANK_MASKF8   (0xf8<<16)

typedef struct _sega_pcm_interface sega_pcm_interface;
struct _sega_pcm_interface
{
	int  bank;
};

/*WRITE8_DEVICE_HANDLER( sega_pcm_w );
READ8_DEVICE_HANDLER( sega_pcm_r );

DEVICE_GET_INFO( segapcm );
#define SOUND_SEGAPCM DEVICE_GET_INFO_NAME( segapcm )*/

#ifdef __cplusplus
extern "C" {
#endif

void SEGAPCM_update(void *chip, stream_sample_t **outputs, int samples);

void * device_start_segapcm(int intf_bank);
void device_stop_segapcm(void *chip);
void device_reset_segapcm(void *chip);

void sega_pcm_w(void *chip, offs_t offset, UINT8 data);
UINT8 sega_pcm_r(void *chip, offs_t offset);
void sega_pcm_write_rom(void *chip, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
						const UINT8* ROMData);

//void sega_pcm_fwrite_romusage(UINT8 ChipID);
void segapcm_set_mute_mask(void *chip, UINT32 MuteMask);

#ifdef __cplusplus
}
#endif
