/*********************************************************/
/*    ricoh RF5C68(or clone) PCM controller              */
/*********************************************************/

#include <memory.h>
#include <stdlib.h>
//#include "sndintrf.h"
//#include "streams.h"
#include "rf5c68.h"
#include <math.h>

#undef NULL
#define NULL	((void *)0)


#define  NUM_CHANNELS    (8)
#define WRITES_PER_SAMPLE	12



typedef struct _pcm_channel pcm_channel;
struct _pcm_channel
{
	UINT8		enable;
	UINT8		env;
	UINT8		pan;
	UINT8		start;
	UINT32		addr;
	UINT16		step;
	UINT16		loopst;
	UINT8		Muted;
};

typedef struct _mem_stream mem_stream;
struct _mem_stream
{
	UINT32 BaseAddr;
	UINT32 EndAddr;
	UINT32 CurAddr;
	const UINT8* MemPnt;
};


typedef struct _rf5c68_state rf5c68_state;
struct _rf5c68_state
{
	//sound_stream *		stream;
	pcm_channel			chan[NUM_CHANNELS];
	UINT8				cbank;
	UINT8				wbank;
	UINT8				enable;
	UINT32				datasize;
	UINT8*				data;
	//void				(*sample_callback)(running_device* device,int channel);
	mem_stream			memstrm;
};


static void rf5c68_mem_stream_flush(rf5c68_state *chip);

/*INLINE rf5c68_state *get_safe_token(const device_config *device)
{
	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type == SOUND);
	assert(sound_get_type(device) == SOUND_RF5C68);
	return (rf5c68_state *)device->token;
}*/

/************************************************/
/*    RF5C68 stream update                      */
/************************************************/

static void memstream_sample_check(rf5c68_state *chip, UINT32 addr)
{
	mem_stream* ms = &chip->memstrm;
	
	if (addr >= ms->CurAddr)
	{
		if (addr - ms->CurAddr <= WRITES_PER_SAMPLE * 5)
		{
			ms->CurAddr -= WRITES_PER_SAMPLE * 2;
			if (ms->CurAddr < ms->BaseAddr)
				ms->CurAddr = ms->BaseAddr;
		}
	}
	else
	{
		if (ms->CurAddr - addr <= WRITES_PER_SAMPLE * 4)
		{
			rf5c68_mem_stream_flush(chip);
		}
	}
	
	return;
}

//static STREAM_UPDATE( rf5c68_update )
void rf5c68_update(void *_chip, stream_sample_t **outputs, int samples)
{
	//rf5c68_state *chip = (rf5c68_state *)param;
	rf5c68_state *chip = (rf5c68_state *) _chip;
	mem_stream* ms = &chip->memstrm;
	stream_sample_t *left = outputs[0];
	stream_sample_t *right = outputs[1];
	int i, j;

	/* start with clean buffers */
	memset(left, 0, samples * sizeof(*left));
	memset(right, 0, samples * sizeof(*right));

	/* bail if not enabled */
	if (!chip->enable)
		return;

	/* loop over channels */
	for (i = 0; i < NUM_CHANNELS; i++)
	{
		pcm_channel *chan = &chip->chan[i];

		/* if this channel is active, accumulate samples */
		if (chan->enable && ! chan->Muted)
		{
			int lv = (chan->pan & 0x0f) * chan->env;
			int rv = ((chan->pan >> 4) & 0x0f) * chan->env;

			/* loop over the sample buffer */
			for (j = 0; j < samples; j++)
			{
				int sample;

				/* trigger sample callback */
				/*if(chip->sample_callback)
				{
					if(((chan->addr >> 11) & 0xfff) == 0xfff)
						chip->sample_callback(chip->device,((chan->addr >> 11)/0x2000));
				}*/

				memstream_sample_check(chip, (chan->addr >> 11) & 0xffff);
				/* fetch the sample and handle looping */
				sample = chip->data[(chan->addr >> 11) & 0xffff];
				if (sample == 0xff)
				{
					chan->addr = chan->loopst << 11;
					sample = chip->data[(chan->addr >> 11) & 0xffff];

					/* if we loop to a loop point, we're effectively dead */
					if (sample == 0xff)
						break;
				}
				chan->addr += chan->step;

				/* add to the buffer */
				if (sample & 0x80)
				{
					sample &= 0x7f;
					left[j] += (sample * lv) >> 5;
					right[j] += (sample * rv) >> 5;
				}
				else
				{
					left[j] -= (sample * lv) >> 5;
					right[j] -= (sample * rv) >> 5;
				}
			}
		}
	}

	if (samples && ms->CurAddr < ms->EndAddr)
	{
		i = WRITES_PER_SAMPLE * samples;
		if (ms->CurAddr + i > ms->EndAddr)
			i = ms->EndAddr - ms->CurAddr;
		
		memcpy(chip->data + ms->CurAddr, ms->MemPnt + (ms->CurAddr - ms->BaseAddr), i);
		ms->CurAddr += i;
	}
	
	// I think, this is completely useless
	/* now clamp and shift the result (output is only 10 bits) */
	/*for (j = 0; j < samples; j++)
	{
		stream_sample_t temp;

		temp = left[j];
		if (temp > 32767) temp = 32767;
		else if (temp < -32768) temp = -32768;
		left[j] = temp & ~0x3f;

		temp = right[j];
		if (temp > 32767) temp = 32767;
		else if (temp < -32768) temp = -32768;
		right[j] = temp & ~0x3f;
	}*/
}


/************************************************/
/*    RF5C68 start                              */
/************************************************/

//static DEVICE_START( rf5c68 )
void * device_start_rf5c68()
{
	//const rf5c68_interface* intf = (const rf5c68_interface*)device->baseconfig().static_config();
	
	/* allocate memory for the chip */
	//rf5c68_state *chip = get_safe_token(device);
	rf5c68_state *chip;
	int chn;
	
	chip = (rf5c68_state *) malloc(sizeof(rf5c68_state));
	if (!chip) return chip;
	
	chip->datasize = 0x10000;
	chip->data = (UINT8*)malloc(chip->datasize);
	
	/* allocate the stream */
	//chip->stream = stream_create(device, 0, 2, device->clock / 384, chip, rf5c68_update);

	/* set up callback */
	/*if(intf != NULL)
		chip->sample_callback = intf->sample_end_callback;
	else
		chip->sample_callback = NULL;*/
	for (chn = 0; chn < NUM_CHANNELS; chn ++)
		chip->chan[chn].Muted = 0x00;
	
	return chip;
}

void device_stop_rf5c68(void *_chip)
{
	rf5c68_state *chip = (rf5c68_state *) _chip;
	free(chip->data);	chip->data = NULL;
	free(chip);
}

void device_reset_rf5c68(void *_chip)
{
	rf5c68_state *chip = (rf5c68_state *) _chip;
	int i;
	pcm_channel* chan;
	mem_stream* ms = &chip->memstrm;
	
	// Clear the PCM memory.
	memset(chip->data, 0x00, chip->datasize);
	
	chip->enable = 0;
	chip->cbank = 0;
	chip->wbank = 0;
	
	/* clear channel registers */
	for (i = 0; i < NUM_CHANNELS; i ++)
	{
		chan = &chip->chan[i];
		chan->enable = 0;
		chan->env = 0;
		chan->pan = 0;
		chan->start = 0;
		chan->addr = 0;
		chan->step = 0;
		chan->loopst = 0;
	}
	
	ms->BaseAddr = 0x0000;
	ms->CurAddr = 0x0000;
	ms->EndAddr = 0x0000;
	ms->MemPnt = NULL;
}

/************************************************/
/*    RF5C68 write register                     */
/************************************************/

//WRITE8_DEVICE_HANDLER( rf5c68_w )
void rf5c68_w(void *_chip, offs_t offset, UINT8 data)
{
	//rf5c68_state *chip = get_safe_token(device);
	rf5c68_state *chip = (rf5c68_state *) _chip;
	pcm_channel *chan = &chip->chan[chip->cbank];
	int i;

	/* force the stream to update first */
	//stream_update(chip->stream);

	/* switch off the address */
	switch (offset)
	{
		case 0x00:	/* envelope */
			chan->env = data;
			break;

		case 0x01:	/* pan */
			chan->pan = data;
			break;

		case 0x02:	/* FDL */
			chan->step = (chan->step & 0xff00) | (data & 0x00ff);
			break;

		case 0x03:	/* FDH */
			chan->step = (chan->step & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x04:	/* LSL */
			chan->loopst = (chan->loopst & 0xff00) | (data & 0x00ff);
			break;

		case 0x05:	/* LSH */
			chan->loopst = (chan->loopst & 0x00ff) | ((data << 8) & 0xff00);
			break;

		case 0x06:	/* ST */
			chan->start = data;
			if (!chan->enable)
				chan->addr = chan->start << (8 + 11);
			break;

		case 0x07:	/* control reg */
			chip->enable = (data >> 7) & 1;
			if (data & 0x40)
				chip->cbank = data & 7;
			else
				chip->wbank = data & 15;
			break;

		case 0x08:	/* channel on/off reg */
			for (i = 0; i < 8; i++)
			{
				chip->chan[i].enable = (~data >> i) & 1;
				if (!chip->chan[i].enable)
					chip->chan[i].addr = chip->chan[i].start << (8 + 11);
			}
			break;
	}
}


/************************************************/
/*    RF5C68 read memory                        */
/************************************************/

//READ8_DEVICE_HANDLER( rf5c68_mem_r )
UINT8 rf5c68_mem_r(void *_chip, offs_t offset)
{
	//rf5c68_state *chip = get_safe_token(device);
	rf5c68_state *chip = (rf5c68_state *) _chip;
	return chip->data[chip->wbank * 0x1000 + offset];
}


/************************************************/
/*    RF5C68 write memory                       */
/************************************************/

//WRITE8_DEVICE_HANDLER( rf5c68_mem_w )
void rf5c68_mem_w(void *_chip, offs_t offset, UINT8 data)
{
	//rf5c68_state *chip = get_safe_token(device);
	rf5c68_state *chip = (rf5c68_state *) _chip;
	rf5c68_mem_stream_flush(chip);
	chip->data[chip->wbank * 0x1000 | offset] = data;
}

static void rf5c68_mem_stream_flush(rf5c68_state *chip)
{
	mem_stream* ms = &chip->memstrm;
	
	if (ms->CurAddr >= ms->EndAddr)
		return;
	
	memcpy(chip->data + ms->CurAddr, ms->MemPnt + (ms->CurAddr - ms->BaseAddr), ms->EndAddr - ms->CurAddr);
	ms->CurAddr = ms->EndAddr;
	
	return;
}

void rf5c68_write_ram(void *_chip, offs_t DataStart, offs_t DataLength, const UINT8* RAMData)
{
	rf5c68_state *chip = (rf5c68_state *) _chip;
	mem_stream* ms = &chip->memstrm;
	UINT16 BytCnt;
	
	if (DataStart >= chip->datasize)
		return;
	if (DataStart + DataLength > chip->datasize)
		DataLength = chip->datasize - DataStart;
	
	//memcpy(chip->data + (chip->wbank * 0x1000 | DataStart), RAMData, DataLength);
	
	rf5c68_mem_stream_flush(chip);
	
	ms->BaseAddr = chip->wbank * 0x1000 | DataStart;
	ms->CurAddr = ms->BaseAddr;
	ms->EndAddr = ms->BaseAddr + DataLength;
	ms->MemPnt = RAMData;
	
	BytCnt = WRITES_PER_SAMPLE;
	if (ms->CurAddr + BytCnt > ms->EndAddr)
		BytCnt = ms->EndAddr - ms->CurAddr;
	
	memcpy(chip->data + ms->CurAddr, ms->MemPnt + (ms->CurAddr - ms->BaseAddr), BytCnt);
	ms->CurAddr += BytCnt;
	
	return;
}


void rf5c68_set_mute_mask(void *_chip, UINT32 MuteMask)
{
	rf5c68_state *chip = (rf5c68_state *) _chip;
	unsigned char CurChn;
	
	for (CurChn = 0; CurChn < NUM_CHANNELS; CurChn ++)
		chip->chan[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}



/**************************************************************************
 * Generic get_info
 **************************************************************************/

/*DEVICE_GET_INFO( rf5c68 )
{
	switch (state)
	{
		// --- the following bits of info are returned as 64-bit signed integers ---
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(rf5c68_state);				break;

		// --- the following bits of info are returned as pointers to data or functions ---
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( rf5c68 );			break;
		case DEVINFO_FCT_STOP:							// Nothing										break;
		case DEVINFO_FCT_RESET:							// Nothing										break;

		// --- the following bits of info are returned as NULL-terminated strings ---
		case DEVINFO_STR_NAME:							strcpy(info->s, "RF5C68");						break;
		case DEVINFO_STR_FAMILY:					strcpy(info->s, "Ricoh PCM");					break;
		case DEVINFO_STR_VERSION:					strcpy(info->s, "1.0");							break;
		case DEVINFO_STR_SOURCE_FILE:						strcpy(info->s, __FILE__);						break;
		case DEVINFO_STR_CREDITS:					strcpy(info->s, "Copyright Nicola Salmoria and the MAME Team"); break;
	}
}*/

/**************** end of file ****************/
