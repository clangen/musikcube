/**********************************************************************************************
 *
 *   OKI MSM6258 ADPCM
 *
 *   TODO:
 *   3-bit ADPCM support
 *   Recording?
 *
 **********************************************************************************************/


//#include "emu.h"
#include "mamedef.h"
#ifdef _DEBUG
#include <stdio.h>
#endif
//#include "streams.h"
#include <math.h>
#include <stdlib.h>
#include "okim6258.h"

#define COMMAND_STOP		(1 << 0)
#define COMMAND_PLAY		(1 << 1)
#define	COMMAND_RECORD		(1 << 2)

#define STATUS_PLAYING		(1 << 1)
#define STATUS_RECORDING	(1 << 2)

static const int dividers[4] = { 1024, 768, 512, 512 };

typedef struct _okim6258_state okim6258_state;
struct _okim6258_state
{
	UINT8  status;

	UINT32 master_clock;	/* master clock frequency */
	UINT32 divider;			/* master clock divider */
	UINT8 adpcm_type;		/* 3/4 bit ADPCM select */
	UINT8 data_in;			/* ADPCM data-in register */
	UINT8 nibble_shift;		/* nibble select */
	//sound_stream *stream;	/* which stream are we playing on? */

	UINT8 output_bits;

	// Valley Bell: Added a small queue to prevent race conditions.
	UINT8 data_buf[2];
	UINT8 data_buf_pos;
	// Data Empty Values:
	//	00 - data written, but not read yet
	//	01 - read data, waiting for next write
	//	02 - tried to read, but had no data
	UINT8 data_empty;
	// Valley Bell: Added pan
	UINT8 pan;
	INT32 last_smpl;
    
	INT32 signal;
	INT32 step;
	
	UINT8 clock_buffer[0x04];
	UINT32 initial_clock;
	UINT8 initial_div;
};

/* step size index shift table */
static const int index_shift[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

/* lookup table for the precomputed difference */
static int diff_lookup[49*16];

/* tables computed? */
static int tables_computed = 0;

/*INLINE okim6258_state *get_safe_token(running_device *device)
{
	assert(device != NULL);
	assert(device->type() == OKIM6258);
	return (okim6258_state *)downcast<legacy_device_base *>(device)->token();
}*/

/**********************************************************************************************

     compute_tables -- compute the difference tables

***********************************************************************************************/

static void compute_tables(void)
{
	/* nibble to bit map */
	static const int nbl2bit[16][4] =
	{
		{ 1, 0, 0, 0}, { 1, 0, 0, 1}, { 1, 0, 1, 0}, { 1, 0, 1, 1},
		{ 1, 1, 0, 0}, { 1, 1, 0, 1}, { 1, 1, 1, 0}, { 1, 1, 1, 1},
		{-1, 0, 0, 0}, {-1, 0, 0, 1}, {-1, 0, 1, 0}, {-1, 0, 1, 1},
		{-1, 1, 0, 0}, {-1, 1, 0, 1}, {-1, 1, 1, 0}, {-1, 1, 1, 1}
	};

	int step, nib;

	if (tables_computed)
		return;
	
	/* loop over all possible steps */
	for (step = 0; step <= 48; step++)
	{
		/* compute the step value */
		int stepval = floor(16.0 * pow(11.0 / 10.0, (double)step));

		/* loop over all nibbles and compute the difference */
		for (nib = 0; nib < 16; nib++)
		{
			diff_lookup[step*16 + nib] = nbl2bit[nib][0] *
				(stepval   * nbl2bit[nib][1] +
				 stepval/2 * nbl2bit[nib][2] +
				 stepval/4 * nbl2bit[nib][3] +
				 stepval/8);
		}
	}

	tables_computed = 1;
}


static INT16 clock_adpcm(okim6258_state *chip, UINT8 nibble)
{
	INT32 max = (1 << (chip->output_bits - 1)) - 1;
	INT32 min = -(1 << (chip->output_bits - 1));

	chip->signal += diff_lookup[chip->step * 16 + (nibble & 15)];

	/* clamp to the maximum */
	if (chip->signal > max)
		chip->signal = max;
	else if (chip->signal < min)
		chip->signal = min;

	/* adjust the step size and clamp */
	chip->step += index_shift[nibble & 7];
	if (chip->step > 48)
		chip->step = 48;
	else if (chip->step < 0)
		chip->step = 0;

	/* return the signal scaled up to 32767 */
	return chip->signal << 4;
}

/**********************************************************************************************

     okim6258_update -- update the sound chip so that it is in sync with CPU execution

***********************************************************************************************/

//static STREAM_UPDATE( okim6258_update )
void okim6258_update(void *_chip, stream_sample_t **outputs, int samples)
{
	okim6258_state *chip = (okim6258_state *)_chip;
	//stream_sample_t *buffer = outputs[0];
	stream_sample_t *bufL = outputs[0];
	stream_sample_t *bufR = outputs[1];
    
	//memset(outputs[0], 0, samples * sizeof(*outputs[0]));
    
	if (chip->status & STATUS_PLAYING)
	{
		int nibble_shift = chip->nibble_shift;
        
		while (samples)
		{
			/* Compute the new amplitude and update the current step */
			//int nibble = (chip->data_in >> nibble_shift) & 0xf;
			int nibble;
			INT16 sample;
			
			if (! nibble_shift)
			{
				// 1st nibble - get data
				if (! chip->data_empty)
				{
					chip->data_in = chip->data_buf[chip->data_buf_pos >> 4];
					chip->data_buf_pos ^= 0x10;
					if ((chip->data_buf_pos >> 4) == (chip->data_buf_pos & 0x0F))
						chip->data_empty ++;
				}
				else
				{
					chip->data_in = 0x80;
					if (chip->data_empty < 0x80)
						chip->data_empty ++;
				}
			}
			nibble = (chip->data_in >> nibble_shift) & 0xf;
            
			/* Output to the buffer */
			//INT16 sample = clock_adpcm(chip, nibble);
			if (chip->data_empty < 0x02)
			{
				sample = clock_adpcm(chip, nibble);
				chip->last_smpl = sample;
			}
			else
			{
				sample = chip->last_smpl;
				// Valley Bell: data_empty behaviour (loosely) ported from XM6
				if (chip->data_empty >= 0x12)
				{
					chip->data_empty -= 0x10;
					if (chip->signal < 0)
						chip->signal ++;
					else if (chip->signal > 0)
						chip->signal --;
				}
			}
            
			nibble_shift ^= 4;
            
			//*buffer++ = sample;
			*bufL++ = (chip->pan & 0x02) ? 0x00 : sample;
			*bufR++ = (chip->pan & 0x01) ? 0x00 : sample;
			samples--;
		}
        
		/* Update the parameters */
		chip->nibble_shift = nibble_shift;
	}
	else
	{
		/* Fill with 0 */
		while (samples--)
		{
			//*buffer++ = 0;
			*bufL++ = 0;
			*bufR++ = 0;
		}
	}
}



/**********************************************************************************************

     state save support for MAME

***********************************************************************************************/

/*static void okim6258_state_save_register(okim6258_state *info, running_device *device)
{
	state_save_register_device_item(device, 0, info->status);
	state_save_register_device_item(device, 0, info->master_clock);
	state_save_register_device_item(device, 0, info->divider);
	state_save_register_device_item(device, 0, info->data_in);
	state_save_register_device_item(device, 0, info->nibble_shift);
	state_save_register_device_item(device, 0, info->signal);
	state_save_register_device_item(device, 0, info->step);
}*/


/**********************************************************************************************

     OKIM6258_start -- start emulation of an OKIM6258-compatible chip

***********************************************************************************************/

//static DEVICE_START( okim6258 )
void * device_start_okim6258(int clock, int divider, int adpcm_type, int output_12bits)
{
	//const okim6258_interface *intf = (const okim6258_interface *)device->baseconfig().static_config();
	//okim6258_state *info = get_safe_token(device);
	okim6258_state *info;

	info = (okim6258_state *) calloc(1, sizeof(okim6258_state));
	
	compute_tables();

	//info->master_clock = device->clock();
    info->initial_clock = clock;
    info->initial_div = divider;
	info->master_clock = clock;
	info->adpcm_type = /*intf->*/adpcm_type;
	info->clock_buffer[0x00] = (clock & 0x000000FF) >>  0;
	info->clock_buffer[0x01] = (clock & 0x0000FF00) >>  8;
	info->clock_buffer[0x02] = (clock & 0x00FF0000) >> 16;
	info->clock_buffer[0x03] = (clock & 0xFF000000) >> 24;

	/* D/A precision is 10-bits but 12-bit data can be output serially to an external DAC */
	info->output_bits = /*intf->*/output_12bits ? 12 : 10;
	info->divider = dividers[/*intf->*/divider];

	//info->stream = stream_create(device, 0, 1, device->clock()/info->divider, info, okim6258_update);

	info->signal = -2;
	info->step = 0;

	//okim6258_state_save_register(info, device);
	
	return info; //return info->master_clock / info->divider;
}


/**********************************************************************************************

     OKIM6258_stop -- stop emulation of an OKIM6258-compatible chip

***********************************************************************************************/

void device_stop_okim6258(void *chip)
{
	okim6258_state *info = (okim6258_state *) chip;

	free(info);
}

//static DEVICE_RESET( okim6258 )
void device_reset_okim6258(void *chip)
{
	//okim6258_state *info = get_safe_token(device);
	okim6258_state *info = (okim6258_state *) chip;

	//stream_update(info->stream);

	info->master_clock = info->initial_clock;
	info->clock_buffer[0x00] = (info->initial_clock & 0x000000FF) >>  0;
	info->clock_buffer[0x01] = (info->initial_clock & 0x0000FF00) >>  8;
	info->clock_buffer[0x02] = (info->initial_clock & 0x00FF0000) >> 16;
	info->clock_buffer[0x03] = (info->initial_clock & 0xFF000000) >> 24;
	info->divider = dividers[info->initial_div];

    
	info->signal = -2;
	info->step = 0;
	info->status = 0;

	// Valley Bell: Added reset of the Data In register.
	info->data_in = 0x00;
	info->data_buf[0] = info->data_buf[1] = 0x00;
	info->data_buf_pos = 0x00;
	info->data_empty = 0xFF;
	info->pan = 0x00;
}


/**********************************************************************************************

     okim6258_set_divider -- set the master clock divider

***********************************************************************************************/

//void okim6258_set_divider(running_device *device, int val)
void okim6258_set_divider(void *chip, int val)
{
	okim6258_state *info = (okim6258_state *) chip;

	info->divider = dividers[val];
	//stream_set_sample_rate(info->stream, info->master_clock / divider);
}


/**********************************************************************************************

     okim6258_set_clock -- set the master clock

***********************************************************************************************/

//void okim6258_set_clock(running_device *device, int val)
void okim6258_set_clock(void *chip, int val)
{
	okim6258_state *info = (okim6258_state *) chip;

	if (val)
	{
		info->master_clock = val;
	}
	else
	{
		info->master_clock =	(info->clock_buffer[0x00] <<  0) |
								(info->clock_buffer[0x01] <<  8) |
								(info->clock_buffer[0x02] << 16) |
								(info->clock_buffer[0x03] << 24);
	}
}


/**********************************************************************************************

     okim6258_get_vclk -- get the VCLK/sampling frequency

***********************************************************************************************/

//int okim6258_get_vclk(running_device *device)
int okim6258_get_vclk(void *chip)
{
	okim6258_state *info = (okim6258_state *) chip;

	return (info->master_clock / info->divider);
}


/**********************************************************************************************

     okim6258_status_r -- read the status port of an OKIM6258-compatible chip

***********************************************************************************************/

//READ8_DEVICE_HANDLER( okim6258_status_r )
UINT8 okim6258_status_r(void *chip, offs_t offset)
{
	//okim6258_state *info = get_safe_token(device);
	okim6258_state *info = (okim6258_state *) chip;

	//stream_update(info->stream);

	return (info->status & STATUS_PLAYING) ? 0x00 : 0x80;
}


/**********************************************************************************************

     okim6258_data_w -- write to the control port of an OKIM6258-compatible chip

***********************************************************************************************/
//WRITE8_DEVICE_HANDLER( okim6258_data_w )
void okim6258_data_w(void *chip, offs_t offset, UINT8 data)
{
	//okim6258_state *info = get_safe_token(device);
	okim6258_state *info = (okim6258_state *) chip;

	/* update the stream */
	//stream_update(info->stream);

	//info->data_in = data;
	//info->nibble_shift = 0;

	if (info->data_empty >= 0x02)
	{
		info->data_buf_pos = 0x00;
		info->data_buf[info->data_buf_pos & 0x0F] = 0x80;
	}
	info->data_buf[info->data_buf_pos & 0x0F] = data;
	info->data_buf_pos ^= 0x01;
	info->data_empty = 0x00;
}


/**********************************************************************************************

     okim6258_ctrl_w -- write to the control port of an OKIM6258-compatible chip

***********************************************************************************************/

//WRITE8_DEVICE_HANDLER( okim6258_ctrl_w )
void okim6258_ctrl_w(void *chip, offs_t offset, UINT8 data)
{
	//okim6258_state *info = get_safe_token(device);
	okim6258_state *info = (okim6258_state *) chip;

	//stream_update(info->stream);

	if (data & COMMAND_STOP)
	{
		info->status &= ~(STATUS_PLAYING | STATUS_RECORDING);
		return;
	}
    
	if (data & COMMAND_PLAY)
	{
		if (!(info->status & STATUS_PLAYING))
		{
			info->status |= STATUS_PLAYING;
            
			/* Also reset the ADPCM parameters */
			//info->signal = -2;
			//info->step = 0;
			//info->nibble_shift = 0;
		}
		//info->signal = -2;
		info->step = 0;
		info->nibble_shift = 0;
	}
	else
	{
		info->status &= ~STATUS_PLAYING;
	}
    
	if (data & COMMAND_RECORD)
	{
		/*logerror("M6258: Record enabled\n");*/
		info->status |= STATUS_RECORDING;
	}
	else
	{
		info->status &= ~STATUS_RECORDING;
	}
}

void okim6258_set_clock_byte(void *chip, UINT8 Byte, UINT8 val)
{
	okim6258_state *info = (okim6258_state *) chip;
	
	info->clock_buffer[Byte] = val;
	
	return;
}

static void okim6258_pan_w(void *chip, UINT8 data)
{
	okim6258_state *info = (okim6258_state *) chip;
	
	info->pan = data;
	
	return;
}


void okim6258_write(void *chip, UINT8 Port, UINT8 Data)
{
	switch(Port)
	{
	case 0x00:
		okim6258_ctrl_w(chip, 0x00, Data);
		break;
	case 0x01:
		okim6258_data_w(chip, 0x00, Data);
		break;
    case 0x02:
        okim6258_pan_w(chip, Data);
        break;
	case 0x08:
	case 0x09:
	case 0x0A:
		okim6258_set_clock_byte(chip, Port & 0x03, Data);
		break;
	case 0x0B:
		okim6258_set_clock_byte(chip, Port & 0x03, Data);
		okim6258_set_clock(chip, 0);
		break;
	case 0x0C:
		okim6258_set_divider(chip, Data);
		break;
	}
}
