#ifndef _DAC_CONTROL_H_
#define _DAC_CONTROL_H_

#ifndef __OSDCOMM_H__
#define __OSDCOMM_H__
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef unsigned long long UINT64; /* unsigned 64bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
typedef signed long long INT64;  /* signed 64bit   */

typedef INT32           stream_sample_t;
typedef UINT32          offs_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

void * device_start_daccontrol(UINT32 samplerate, void *context);
void device_stop_daccontrol(void * chip);

void daccontrol_update(void * chip, UINT32 base_clock, UINT32 samples);
void device_reset_daccontrol(void * chip);
void daccontrol_setup_chip(void * chip, UINT8 ChType, UINT8 ChNum, UINT16 Command);
void daccontrol_set_data(void *chip, const UINT8* Data, UINT32 DataLen, UINT8 StepSize, UINT8 StepBase);
void daccontrol_set_frequency(void *chip, UINT32 Frequency);
void daccontrol_start(void *chip, UINT32 DataPos, UINT8 LenMode, UINT32 Length);
void daccontrol_stop(void *chip);

#define DCTRL_LMODE_IGNORE	0x00
#define DCTRL_LMODE_CMDS	0x01
#define DCTRL_LMODE_MSEC	0x02
#define DCTRL_LMODE_TOEND	0x03
#define DCTRL_LMODE_BYTES	0x0F

#ifdef __cplusplus
}
#endif

#endif
