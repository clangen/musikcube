/* C140.h */

#pragma once

#ifndef __OSDCOMM_H__
#define __OSDCOMM_H__
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */

typedef INT32           stream_sample_t;
typedef UINT32          offs_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void c140_update(void * chip, stream_sample_t **outputs, int samples);
void * device_start_c140(int sample_rate, int clock, int banking_type);
void device_stop_c140(void * chip);
void device_reset_c140(void * chip);

//READ8_DEVICE_HANDLER( c140_r );
//WRITE8_DEVICE_HANDLER( c140_w );
UINT8 c140_r(void * chip, offs_t offset);
void c140_w(void * chip, offs_t offset, UINT8 data);

//void c140_set_base(device_t *device, void *base);
void c140_set_base(void *chip, void *base);

enum
{
	C140_TYPE_SYSTEM2,
	C140_TYPE_SYSTEM21_A,
	C140_TYPE_SYSTEM21_B,
	C140_TYPE_ASIC219
};

/*typedef struct _c140_interface c140_interface;
struct _c140_interface {
    int banking_type;
};*/


void c140_write_rom(void * chip, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
					const UINT8* ROMData);

void c140_set_mute_mask(void * chip, UINT32 MuteMask);

#ifdef __cplusplus
}
#endif

//DECLARE_LEGACY_SOUND_DEVICE(C140, c140);
