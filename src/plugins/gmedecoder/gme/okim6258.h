#pragma once

#include "mamedef.h"

//#include "devlegcy.h"

/* an interface for the OKIM6258 and similar chips */

/*typedef struct _okim6258_interface okim6258_interface;
struct _okim6258_interface
{
	int divider;
	int adpcm_type;
	int output_12bits;
};*/

#ifdef __cplusplus
extern "C" {
#endif

#define FOSC_DIV_BY_1024	0
#define FOSC_DIV_BY_768		1
#define FOSC_DIV_BY_512		2

#define TYPE_3BITS      	0
#define TYPE_4BITS			1

#define	OUTPUT_10BITS		0
#define	OUTPUT_12BITS		1

void okim6258_update(void *, stream_sample_t **outputs, int samples);
void * device_start_okim6258(int clock, int divider, int adpcm_type, int output_12bits);
void device_stop_okim6258(void *);
void device_reset_okim6258(void *);

//void okim6258_set_divider(running_device *device, int val);
//void okim6258_set_clock(running_device *device, int val);
//int okim6258_get_vclk(running_device *device);

void okim6258_set_divider(void *, int val);
void okim6258_set_clock(void *, int val);
int okim6258_get_vclk(void *);

//READ8_DEVICE_HANDLER( okim6258_status_r );
//WRITE8_DEVICE_HANDLER( okim6258_data_w );
//WRITE8_DEVICE_HANDLER( okim6258_ctrl_w );

UINT8 okim6258_status_r(void *, offs_t offset);
void okim6258_data_w(void *, offs_t offset, UINT8 data);
void okim6258_ctrl_w(void *, offs_t offset, UINT8 data);
void okim6258_write(void *, UINT8 Port, UINT8 Data);

//DECLARE_LEGACY_SOUND_DEVICE(OKIM6258, okim6258);

#ifdef __cplusplus
}
#endif
