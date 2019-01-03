/***************************************************************************

    Konami 051649 - SCC1 sound as used in Haunted Castle, City Bomber

    This file is pieced together by Bryan McPhail from a combination of
    Namco Sound, Amuse by Cab, Haunted Castle schematics and whoever first
    figured out SCC!

    The 051649 is a 5 channel sound generator, each channel gets it's
    waveform from RAM (32 bytes per waveform, 8 bit signed data).

    This sound chip is the same as the sound chip in some Konami
    megaROM cartridges for the MSX. It is actually well researched
    and documented:

        http://www.msxnet.org/tech/scc

    Thanks to Sean Young (sean@mess.org) for some bugfixes.

    K052539 is equivalent to this chip except channel 5 does not share
    waveforms with channel 4.

***************************************************************************/

#include "mamedef.h"
#include <stdlib.h>
#include <memory.h>
//#include "emu.h"
//#include "streams.h"
#include "k051649.h"

#define FREQBASEBITS	16

/* this structure defines the parameters for a channel */
typedef struct
{
	unsigned long counter;
	int frequency;
	int volume;
	int key;
	signed char waveform[32];		/* 19991207.CAB */
	UINT8 Muted;
} k051649_sound_channel;

typedef struct _k051649_state k051649_state;
struct _k051649_state
{
	k051649_sound_channel channel_list[5];

	/* global sound parameters */
	//sound_stream * stream;
	int mclock,rate;

	/* mixer tables and internal buffers */
	INT16 *mixer_table;
	INT16 *mixer_lookup;
	short *mixer_buffer;

	int f[10];
	int cur_reg;
};

/*INLINE k051649_state *get_safe_token(running_device *device)
{
	assert(device != NULL);
	assert(device->type() == K051649);
	return (k051649_state *)downcast<legacy_device_base *>(device)->token();
}*/

/* build a table to divide by the number of voices */
static void make_mixer_table(/*running_machine *machine,*/ k051649_state *info, int voices)
{
	int count = voices * 256;
	int i;
	int gain = 8;

	/* allocate memory */
	//info->mixer_table = auto_alloc_array(machine, INT16, 512 * voices);
	info->mixer_table = (INT16*)malloc(sizeof(INT16) * 512 * voices);

	/* find the middle of the table */
	info->mixer_lookup = info->mixer_table + (256 * voices);

	/* fill in the table - 16 bit case */
	for (i = 0; i < count; i++)
	{
		int val = i * gain * 16 / voices;
		if (val > 32767) val = 32767;
		info->mixer_lookup[ i] = val;
		info->mixer_lookup[-i] = -val;
	}
}


/* generate sound to the mix buffer */
//static STREAM_UPDATE( k051649_update )
void k051649_update(void *chip, stream_sample_t **outputs, int samples)
{
	k051649_state *info = (k051649_state *) chip;
	k051649_sound_channel *voice=info->channel_list;
	stream_sample_t *buffer = outputs[0];
	stream_sample_t *buffer2 = outputs[1];
	short *mix;
	int i,v,f,j,k;

	/* zap the contents of the mixer buffer */
	memset(info->mixer_buffer, 0, samples * sizeof(short));

	for (j=0; j<5; j++) {
		v=voice[j].volume;
		f=voice[j].frequency;
		k=voice[j].key;
		/* SY 20040109: the SCC produces no sound for freq < 9 */
		if (v && f > 8 && k && ! voice[j].Muted)
		{
			const signed char *w = voice[j].waveform;			/* 19991207.CAB */
			int c=voice[j].counter;

			mix = info->mixer_buffer;

			/* add our contribution */
			for (i = 0; i < samples; i++)
			{
				int offs;

				/* Amuse source:  Cab suggests this method gives greater resolution */
				/* Sean Young 20010417: the formula is really: f = clock/(16*(f+1))*/
				c+=(long)((((float)info->mclock / (float)((f+1) * 16))*(float)(1<<FREQBASEBITS)) / (float)(info->rate / 32));
				offs = (c >> 16) & 0x1f;
				*mix++ += (w[offs] * v)>>3;
			}

			/* update the counter for this voice */
			voice[j].counter = c;
		}
	}

	/* mix it down */
	mix = info->mixer_buffer;
	for (i = 0; i < samples; i++)
		*buffer++ = *buffer2++ = info->mixer_lookup[*mix++];
}

//static DEVICE_START( k051649 )
void * device_start_k051649(int clock)
{
	//k051649_state *info = get_safe_token(device);
	k051649_state *info;
	UINT8 CurChn;

	info = (k051649_state *) calloc(1, sizeof(k051649_state));
	/* get stream channels */
	//info->rate = device->clock()/16;
	info->rate = clock/16;
	//info->stream = stream_create(device, 0, 1, info->rate, info, k051649_update);
	//info->mclock = device->clock();
	info->mclock = clock;

	/* allocate a buffer to mix into - 1 second's worth should be more than enough */
	//info->mixer_buffer = auto_alloc_array(device->machine, short, 2 * info->rate);
	info->mixer_buffer = (short*)malloc(sizeof(short) * info->rate);

	/* build the mixer table */
	//make_mixer_table(device->machine, info, 5);
	make_mixer_table(info, 5);
	
	for (CurChn = 0; CurChn < 5; CurChn ++)
		info->channel_list[CurChn].Muted = 0x00;
	
	return info; //return info->rate;
}

void device_stop_k051649(void *chip)
{
	k051649_state *info = (k051649_state *) chip;
	
	free(info->mixer_buffer);
	free(info->mixer_table);
	free(info);
}

//static DEVICE_RESET( k051649 )
void device_reset_k051649(void *chip)
{
	k051649_state *info = (k051649_state *) chip;
	k051649_sound_channel *voice = info->channel_list;
	int i;

	/* reset all the voices */
	for (i = 0; i < 5; i++) {
		voice[i].frequency = 0;
		voice[i].volume = 0;
		voice[i].counter = 0;
	}
}

/********************************************************************************/

//WRITE8_DEVICE_HANDLER( k051649_waveform_w )
void k051649_waveform_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	//stream_update(info->stream);
	info->channel_list[offset>>5].waveform[offset&0x1f]=data;
	/* SY 20001114: Channel 5 shares the waveform with channel 4 */
	if (offset >= 0x60)
		info->channel_list[4].waveform[offset&0x1f]=data;
}

//READ8_DEVICE_HANDLER ( k051649_waveform_r )
UINT8 k051649_waveform_r(void *chip, offs_t offset)
{
	k051649_state *info = (k051649_state *) chip;
	return info->channel_list[offset>>5].waveform[offset&0x1f];
}

/* SY 20001114: Channel 5 doesn't share the waveform with channel 4 on this chip */
//WRITE8_DEVICE_HANDLER( k052539_waveform_w )
void k052539_waveform_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	//stream_update(info->stream);
	info->channel_list[offset>>5].waveform[offset&0x1f]=data;
}

//WRITE8_DEVICE_HANDLER( k051649_volume_w )
void k051649_volume_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	//stream_update(info->stream);
	info->channel_list[offset&0x7].volume=data&0xf;
}

//WRITE8_DEVICE_HANDLER( k051649_frequency_w )
void k051649_frequency_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	info->f[offset]=data;

	//stream_update(info->stream);
	info->channel_list[offset>>1].frequency=(info->f[offset&0xe] + (info->f[offset|1]<<8))&0xfff;
}

//WRITE8_DEVICE_HANDLER( k051649_keyonoff_w )
void k051649_keyonoff_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	//stream_update(info->stream);
	info->channel_list[0].key=data&1;
	info->channel_list[1].key=data&2;
	info->channel_list[2].key=data&4;
	info->channel_list[3].key=data&8;
	info->channel_list[4].key=data&16;
}

void k051649_w(void *chip, offs_t offset, UINT8 data)
{
	k051649_state *info = (k051649_state *) chip;
	
	switch(offset & 1)
	{
	case 0x00:
		info->cur_reg = data;
		break;
	case 0x01:
		switch(offset >> 1)
		{
		case 0x00:
			k051649_waveform_w(info, info->cur_reg, data);
			break;
		case 0x01:
			k051649_frequency_w(info, info->cur_reg, data);
			break;
		case 0x02:
			k051649_volume_w(info, info->cur_reg, data);
			break;
		case 0x03:
			k051649_keyonoff_w(info, info->cur_reg, data);
			break;
		case 0x04:
			k052539_waveform_w(info, info->cur_reg, data);
			break;
		}
		break;
	}
	
	return;
}


void k051649_set_mute_mask(void *chip, UINT32 MuteMask)
{
	k051649_state *info = (k051649_state *) chip;
	UINT8 CurChn;
	
	for (CurChn = 0; CurChn < 5; CurChn ++)
		info->channel_list[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
}
