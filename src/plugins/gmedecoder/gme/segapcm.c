/*********************************************************/
/*    SEGA 16ch 8bit PCM                                 */
/*********************************************************/

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
//#include "sndintrf.h"
//#include "streams.h"
#include "segapcm.h"


typedef struct _segapcm_state segapcm_state;
struct _segapcm_state
{
	UINT8  *ram;
	UINT8 low[16];
	UINT32 ROMSize;
	UINT8 *rom;
#ifdef _DEBUG
	UINT8 *romusage;
#endif
	int bankshift;
	int bankmask;
	int rgnmask;
	sega_pcm_interface intf;
	UINT8 Muted[16];
	//sound_stream * stream;
};

UINT8 SegaPCM_NewCore = 0x00;

/*INLINE segapcm_state *get_safe_token(const device_config *device)
{
	assert(device != NULL);
	assert(device->token != NULL);
	assert(device->type == SOUND);
	assert(sound_get_type(device) == SOUND_SEGAPCM);
	return (segapcm_state *)device->token;
}*/

//static STREAM_UPDATE( SEGAPCM_update )
void SEGAPCM_update(void *chip, stream_sample_t **outputs, int samples)
{
	//segapcm_state *spcm = (segapcm_state *)param;
	segapcm_state *spcm = (segapcm_state *) chip;
	int rgnmask = spcm->rgnmask;
	int ch;

	/* clear the buffers */
	memset(outputs[0], 0, samples*sizeof(stream_sample_t));
	memset(outputs[1], 0, samples*sizeof(stream_sample_t));

	// reg      function
	// ------------------------------------------------
	// 0x00     ?
	// 0x01     ?
	// 0x02     volume left
	// 0x03     volume right
	// 0x04     loop address (08-15)
	// 0x05     loop address (16-23)
	// 0x06     end address
	// 0x07     address delta
	// 0x80     ?
	// 0x81     ?
	// 0x82     ?
	// 0x83     ?
	// 0x84     current address (08-15), 00-07 is internal?
	// 0x85     current address (16-23)
	// 0x86     bit 0: channel disable?
	//          bit 1: loop disable
	//          other bits: bank
	// 0x87     ?

	/* loop over channels */
	for (ch = 0; ch < 16; ch++)
	{
if (! SegaPCM_NewCore)
{
		/* only process active channels */
		if (!(spcm->ram[0x86+8*ch] & 1) && ! spcm->Muted[ch])
		{
			UINT8 *base = spcm->ram+8*ch;
			UINT8 flags = base[0x86];
			const UINT8 *rom = spcm->rom + ((flags & spcm->bankmask) << spcm->bankshift);
#ifdef _DEBUG
			const UINT8 *romusage = spcm->romusage + ((flags & spcm->bankmask) << spcm->bankshift);
#endif
			UINT32 addr = (base[5] << 16) | (base[4] << 8) | spcm->low[ch];
			UINT16 loop = (base[0x85] << 8) | base[0x84];
			UINT8 end = base[6] + 1;
			UINT8 delta = base[7];
			UINT8 voll = base[2];
			UINT8 volr = base[3];
			int i;

			/* loop over samples on this channel */
			for (i = 0; i < samples; i++)
			{
				INT8 v = 0;

				/* handle looping if we've hit the end */
				if ((addr >> 16) == end)
				{
					if (!(flags & 2))
						addr = loop << 8;
					else
					{
						flags |= 1;
						break;
					}
				}

				/* fetch the sample */
				v = rom[(addr >> 8) & rgnmask] - 0x80;
#ifdef _DEBUG
				if (romusage[(addr >> 8) & rgnmask] == 0xFF && (voll || volr))
					printf("Access to empty ROM section! (0x%06lX)\n",
							((flags & spcm->bankmask) << spcm->bankshift) + (addr >> 8) & rgnmask);
#endif

				/* apply panning and advance */
				outputs[0][i] += v * voll;
				outputs[1][i] += v * volr;
				addr += delta;
			}

			/* store back the updated address and info */
			base[0x86] = flags;
			base[4] = addr >> 8;
			base[5] = addr >> 16;
			spcm->low[ch] = flags & 1 ? 0 : addr;
		}
}
else
{
		UINT8 *regs = spcm->ram+8*ch;

		/* only process active channels */
		if (!(regs[0x86] & 1) && ! spcm->Muted[ch])
		{
			const UINT8 *rom = spcm->rom + ((regs[0x86] & spcm->bankmask) << spcm->bankshift);
#ifdef _DEBUG
			const UINT8 *romusage = spcm->romusage + ((regs[0x86] & spcm->bankmask) << spcm->bankshift);
#endif
			UINT32 addr = (regs[0x85] << 16) | (regs[0x84] << 8) | spcm->low[ch];
			UINT32 loop = (regs[0x05] << 16) | (regs[0x04] << 8);
			UINT8 end = regs[6] + 1;
			int i;

			/* loop over samples on this channel */
			for (i = 0; i < samples; i++)
			{
				INT8 v = 0;

				/* handle looping if we've hit the end */
				if ((addr >> 16) == end)
				{
					if (regs[0x86] & 2)
					{
						regs[0x86] |= 1;
						break;
					}
					else addr = loop;
				}

				/* fetch the sample */
				v = rom[(addr >> 8) & rgnmask] - 0x80;
#ifdef _DEBUG
				if (romusage[(addr >> 8) & rgnmask] == 0xFF && (regs[2] || regs[3]))
					printf("Access to empty ROM section! (0x%06lX)\n",
							((regs[0x86] & spcm->bankmask) << spcm->bankshift) + (addr >> 8) & rgnmask);
#endif

				/* apply panning and advance */
				outputs[0][i] += v * regs[2];
				outputs[1][i] += v * regs[3];
				addr = (addr + regs[7]) & 0xffffff;
			}

			/* store back the updated address */
			regs[0x84] = addr >> 8;
			regs[0x85] = addr >> 16;
			spcm->low[ch] = regs[0x86] & 1 ? 0 : addr;
		}
}
	}
}

//static DEVICE_START( segapcm )
void * device_start_segapcm(int intf_bank)
{
	const UINT32 STD_ROM_SIZE = 0x80000;
	//const sega_pcm_interface *intf = (const sega_pcm_interface *)device->static_config;
	sega_pcm_interface *intf;
	int mask, rom_mask, len;
	//segapcm_state *spcm = get_safe_token(device);
	segapcm_state *spcm;

	spcm = (segapcm_state *) malloc(sizeof(segapcm_state));
	if (!spcm) return spcm;

	intf = &spcm->intf;
	intf->bank = intf_bank;
	
	//spcm->rom = (const UINT8 *)device->region;
	//spcm->ram = auto_alloc_array(device->machine, UINT8, 0x800);
	spcm->ROMSize = STD_ROM_SIZE;
	spcm->rom = (UINT8*) malloc(STD_ROM_SIZE);
#ifdef _DEBUG
	spcm->romusage = (UINT8*) malloc(STD_ROM_SIZE);
#endif
	spcm->ram = (UINT8*) malloc(0x800);

	memset(spcm->rom, 0xFF, STD_ROM_SIZE);
#ifdef _DEBUG
	memset(spcm->romusage, 0xFF, STD_ROM_SIZE);
#endif
	//memset(spcm->ram, 0xff, 0x800);	// RAM Clear is done at device_reset

	spcm->bankshift = (UINT8)(intf->bank);
	mask = intf->bank >> 16;
	if(!mask)
		mask = BANK_MASK7>>16;

	len = STD_ROM_SIZE;
	spcm->rgnmask = len - 1;
	for(rom_mask = 1; rom_mask < len; rom_mask *= 2);
	rom_mask--;

	spcm->bankmask = mask & (rom_mask >> spcm->bankshift);

	//spcm->stream = stream_create(device, 0, 2, device->clock / 128, spcm, SEGAPCM_update);

	//state_save_register_device_item_array(device, 0, spcm->low);
	//state_save_register_device_item_pointer(device, 0, spcm->ram, 0x800);
	
	for (mask = 0; mask < 16; mask ++)
		spcm->Muted[mask] = 0x00;
	
	return spcm;
}

//static DEVICE_STOP( segapcm )
void device_stop_segapcm(void *chip)
{
	//segapcm_state *spcm = get_safe_token(device);
	segapcm_state *spcm = (segapcm_state *) chip;
	free(spcm->rom);	spcm->rom = NULL;
#ifdef _DEBUG
	free(spcm->romusage);
#endif
	free(spcm->ram);

	free(spcm);
}

//static DEVICE_RESET( segapcm )
void device_reset_segapcm(void *chip)
{
	//segapcm_state *spcm = get_safe_token(device);
	segapcm_state *spcm = (segapcm_state *) chip;
	
	memset(spcm->ram, 0xFF, 0x800);
	
	return;
}


//WRITE8_DEVICE_HANDLER( sega_pcm_w )
void sega_pcm_w(void *chip, offs_t offset, UINT8 data)
{
	//segapcm_state *spcm = get_safe_token(device);
	segapcm_state *spcm = (segapcm_state *) chip;
	//stream_update(spcm->stream);

	spcm->ram[offset & 0x07ff] = data;
}

//READ8_DEVICE_HANDLER( sega_pcm_r )
UINT8 sega_pcm_r(void *chip, offs_t offset)
{
	//segapcm_state *spcm = get_safe_token(device);
	segapcm_state *spcm = (segapcm_state *) chip;
	//stream_update(spcm->stream);
	return spcm->ram[offset & 0x07ff];
}

void sega_pcm_write_rom(void *chip, offs_t ROMSize, offs_t DataStart, offs_t DataLength,
						const UINT8* ROMData)
{
	segapcm_state *spcm = (segapcm_state *) chip;
	
	if (spcm->ROMSize != ROMSize)
	{
		unsigned long int mask, rom_mask;
		
		spcm->rom = (UINT8*)realloc(spcm->rom, ROMSize);
#ifdef _DEBUG
		spcm->romusage = (UINT8*)realloc(spcm->romusage, ROMSize);
#endif
		spcm->ROMSize = ROMSize;
		memset(spcm->rom, 0xFF, ROMSize);
#ifdef _DEBUG
		memset(spcm->romusage, 0xFF, ROMSize);
#endif
		
		// recalculate bankmask
		mask = spcm->intf.bank >> 16;
		if (! mask)
			mask = BANK_MASK7 >> 16;
		
		spcm->rgnmask = ROMSize - 1;
		for (rom_mask = 1; rom_mask < ROMSize; rom_mask *= 2);
		rom_mask --;
		
		spcm->bankmask = mask & (rom_mask >> spcm->bankshift);
	}
	if (DataStart > ROMSize)
		return;
	if (DataStart + DataLength > ROMSize)
		DataLength = ROMSize - DataStart;
	
	memcpy(spcm->rom + DataStart, ROMData, DataLength);
#ifdef _DEBUG
	memset(spcm->romusage + DataStart, 0x00, DataLength);
#endif
	
	return;
}


/*void sega_pcm_fwrite_romusage(UINT8 ChipID)
{
	segapcm_state *spcm = &SPCMData[ChipID];
	
	FILE* hFile;
	
	hFile = fopen("SPCM_ROMUsage.bin", "wb");
	if (hFile == NULL)
		return;
	
	fwrite(spcm->romusage, 0x01, spcm->ROMSize, hFile);
	
	fclose(hFile);
	return;
}*/

void segapcm_set_mute_mask(void *chip, UINT32 MuteMask)
{
	segapcm_state *spcm = (segapcm_state *) chip;
	unsigned char CurChn;
	
	for (CurChn = 0; CurChn < 16; CurChn ++)
		spcm->Muted[CurChn] = (MuteMask >> CurChn) & 0x01;
	
	return;
}


/**************************************************************************
 * Generic get_info
 **************************************************************************/

/*DEVICE_GET_INFO( segapcm )
{
	switch (state)
	{
		// --- the following bits of info are returned as 64-bit signed integers ---
		case DEVINFO_INT_TOKEN_BYTES:					info->i = sizeof(segapcm_state);				break;

		// --- the following bits of info are returned as pointers to data or functions ---
		case DEVINFO_FCT_START:							info->start = DEVICE_START_NAME( segapcm );		break;
		case DEVINFO_FCT_STOP:							// Nothing									break;
		case DEVINFO_FCT_RESET:							// Nothing									break;

		// --- the following bits of info are returned as NULL-terminated strings ---
		case DEVINFO_STR_NAME:							strcpy(info->s, "Sega PCM");					break;
		case DEVINFO_STR_FAMILY:					strcpy(info->s, "Sega custom");					break;
		case DEVINFO_STR_VERSION:					strcpy(info->s, "1.0");							break;
		case DEVINFO_STR_SOURCE_FILE:						strcpy(info->s, __FILE__);						break;
		case DEVINFO_STR_CREDITS:					strcpy(info->s, "Copyright Nicola Salmoria and the MAME Team"); break;
	}
}*/
