/***********************************************************/
/*                                                         */
/* PCM.C : PCM RF5C164 emulator                            */
/*                                                         */
/* This source is a part of Gens project                   */
/* Written by Stéphane Dallongeville (gens@consolemul.com) */
/* Copyright (c) 2002 by Stéphane Dallongeville            */
/*                                                         */
/***********************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "scd_pcm.h"
int  PCM_Init(void *chip, int Rate);
void PCM_Set_Rate(void *chip, int Rate);
void PCM_Reset(void *chip);
void PCM_Write_Reg(void *chip, unsigned int Reg, unsigned int Data);
int  PCM_Update(void *chip, int **buf, int Length);

#define PCM_STEP_SHIFT 11

static unsigned char VolTabIsInit = 0x00;
static int PCM_Volume_Tab[256 * 256];

//unsigned char Ram_PCM[64 * 1024];
//int PCM_Enable;


/**
 * PCM_Init(): Initialize the PCM chip.
 * @param Rate Sample rate.
 * @return 0 if successful.
 */
int PCM_Init(void *_chip, int Rate)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	int i, j, out;
	
	if (! VolTabIsInit)
	{
		for (i = 0; i < 0x100; i++)
		{
			for (j = 0; j < 0x100; j++)
			{
				if (i & 0x80)
				{
					out = -(i & 0x7F);
					out *= j;
					PCM_Volume_Tab[(j << 8) + i] = out;
				}
				else
				{
					out = i * j;
					PCM_Volume_Tab[(j << 8) + i] = out;
				}
			}
		}
		VolTabIsInit = 0x01;
	}
	
	for (i = 0; i < 8; i ++)
		chip->Channel[i].Muted = 0x00;
	
	chip->RAMSize = 64 * 1024;
	chip->RAM = (unsigned char*)malloc(chip->RAMSize);
	PCM_Reset(chip);
	PCM_Set_Rate(chip, Rate);
	
	return 0;
}


/**
 * PCM_Reset(): Reset the PCM chip.
 */
void PCM_Reset(void *_chip)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	int i;
	struct pcm_chan_* chan;
	
	// Clear the PCM memory.
	memset(chip->RAM, 0x00, chip->RAMSize);
	
	chip->Enable = 0;
	chip->Cur_Chan = 0;
	chip->Bank = 0;
	
	/* clear channel registers */
	for (i = 0; i < 8; i++)
	{
		chan = &chip->Channel[i];
		chan->Enable = 0;
		chan->ENV = 0;
		chan->PAN = 0;
		chan->St_Addr = 0;
		chan->Addr = 0;
		chan->Loop_Addr = 0;
		chan->Step = 0;
		chan->Step_B = 0;
		chan->Data = 0;
	}
}


/**
 * PCM_Set_Rate(): Change the PCM sample rate.
 * @param Rate New sample rate.
 */
void PCM_Set_Rate(void *_chip, int Rate)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	int i;
	
	if (Rate == 0)
		return;
	
	//chip->Rate = (float) (32 * 1024) / (float) Rate;
	chip->Rate = (float) (31.8 * 1024) / (float) Rate;
	
	for (i = 0; i < 8; i++)
	{
		chip->Channel[i].Step =
			(int) ((float) chip->Channel[i].Step_B * chip->Rate);
	}
}


/**
 * PCM_Write_Reg(): Write to a PCM register.
 * @param Reg Register ID.
 * @param Data Data to write.
 */
void PCM_Write_Reg(void *_chip, unsigned int Reg, unsigned int Data)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	int i;
	struct pcm_chan_* chan = &chip->Channel[chip->Cur_Chan];
	
	Data &= 0xFF;
	
	switch (Reg)
	{
		case 0x00:
			/* evelope register */
			chan->ENV = Data;
			chan->MUL_L = (Data * (chan->PAN & 0x0F)) >> 5;
			chan->MUL_R = (Data * (chan->PAN >> 4)) >> 5;
		break;
		
		case 0x01:
			/* pan register */
			chan->PAN = Data;
			chan->MUL_L = ((Data & 0x0F) * chan->ENV) >> 5;
			chan->MUL_R = ((Data >> 4) * chan->ENV) >> 5;
			break;
		
		case 0x02:
			/* frequency step (LB) registers */
			chan->Step_B &= 0xFF00;
			chan->Step_B += Data;
			chan->Step = (int)((float)chan->Step_B * chip->Rate);
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Step low = %.2X   Step calculated = %.8X",
			//	Data, chan->Step);
			break;
		
		case 0x03:
			/* frequency step (HB) registers */
			chan->Step_B &= 0x00FF;
			chan->Step_B += Data << 8;
			chan->Step = (int)((float)chan->Step_B * chip->Rate);
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Step high = %.2X   Step calculated = %.8X",
			//	Data, chan->Step);
			break;
		
		case 0x04:
			chan->Loop_Addr &= 0xFF00;
			chan->Loop_Addr += Data;			
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Loop low = %.2X   Loop = %.8X",
			//	Data, chan->Loop_Addr);
			break;
		
		case 0x05:
			/* loop address registers */
			chan->Loop_Addr &= 0x00FF;
			chan->Loop_Addr += Data << 8;
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Loop high = %.2X   Loop = %.8X",
			//	Data, chan->Loop_Addr);
			break;
		
		case 0x06:
			/* start address registers */
			chan->St_Addr = Data << (PCM_STEP_SHIFT + 8);
			//chan->Addr = chan->St_Addr;
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Start addr = %.2X   New Addr = %.8X",
			//	Data, chan->Addr);
			break;
		
		case 0x07:
			/* control register */
			/* mod is H */
			if (Data & 0x40)
			{
				/* select channel */
				chip->Cur_Chan = Data & 0x07;
			}
			/* mod is L */
			else
			{
				/* pcm ram bank select */
				chip->Bank = (Data & 0x0F) << 12;
			}
			
			/* sounding bit */
			if (Data & 0x80)
				chip->Enable = 0xFF;	// Used as mask
			else
				chip->Enable = 0;
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"General Enable = %.2X", Data);
			break;
		
		case 0x08:
			/* sound on/off register */
			Data ^= 0xFF;
			
			//LOG_MSG(pcm, LOG_MSG_LEVEL_DEBUG1,
			//	"Channel Enable = %.2X", Data);
			
			for (i = 0; i < 8; i++)
			{
				chan = &chip->Channel[i];
				if (!chan->Enable)
					chan->Addr = chan->St_Addr;
			}
			
			for (i = 0; i < 8; i++)
			{
				chip->Channel[i].Enable = Data & (1 << i);
			}
	}
}


/**
 * PCM_Update(): Update the PCM buffer.
 * @param buf PCM buffer.
 * @param Length Buffer length.
 */
int PCM_Update(void *_chip, int **buf, int Length)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	int i, j;
	int *bufL, *bufR;		//, *volL, *volR;
	unsigned int Addr, k;
	struct pcm_chan_ *CH;
	
	bufL = buf[0];
	bufR = buf[1];
	
	// clear buffers
	memset(bufL, 0, Length * sizeof(int));
	memset(bufR, 0, Length * sizeof(int));

	// if PCM disable, no sound
	if (!chip->Enable)
		return 1;
	
#if 0
	// faster for short update
	for (j = 0; j < Length; j++)
	{
		for (i = 0; i < 8; i++)
		{
			CH = &(chip->Channel[i]);

			// only loop when sounding and on
			if (CH->Enable && ! CH->Muted)
			{
				Addr = CH->Addr >> PCM_STEP_SHIFT;

				if (Addr & 0x10000)
				{
					for(k = CH->Old_Addr; k < 0x10000; k++)
					{
						if (chip->RAM[k] == 0xFF)
						{
							CH->Old_Addr = Addr = CH->Loop_Addr;
							CH->Addr = Addr << PCM_STEP_SHIFT;
							break;
						}
					}

					if (Addr & 0x10000)
					{
						//CH->Addr -= CH->Step;
						CH->Enable = 0;
						break;
					}
				}
				else
				{
					for(k = CH->Old_Addr; k <= Addr; k++)
					{
						if (chip->RAM[k] == 0xFF)
						{
							CH->Old_Addr = Addr = CH->Loop_Addr;
							CH->Addr = Addr << PCM_STEP_SHIFT;
							break;
						}
					}
				}
				
				// test for loop signal
				if (chip->RAM[Addr] == 0xFF)
				{
					Addr = CH->Loop_Addr;
					CH->Addr = Addr << PCM_STEP_SHIFT;
				}
				
				if (chip->RAM[Addr] & 0x80)
				{
					CH->Data = chip->RAM[Addr] & 0x7F;
					bufL[j] -= CH->Data * CH->MUL_L;
					bufR[j] -= CH->Data * CH->MUL_R;
				}
				else
				{
					CH->Data = chip->RAM[Addr];
					bufL[j] += CH->Data * CH->MUL_L;
					bufR[j] += CH->Data * CH->MUL_R;
				}
				
				// update address register
				//CH->Addr = (CH->Addr + CH->Step) & 0x7FFFFFF;
				CH->Addr += CH->Step;
				CH->Old_Addr = Addr + 1;
			}
		}
	}
#endif

#if 1
	// for long update
	for (i = 0; i < 8; i++)
	{
		CH = &(chip->Channel[i]);
		
		// only loop when sounding and on
		if (CH->Enable && ! CH->Muted)
		{
			Addr = CH->Addr >> PCM_STEP_SHIFT;
			//volL = &(PCM_Volume_Tab[CH->MUL_L << 8]);
			//volR = &(PCM_Volume_Tab[CH->MUL_R << 8]);
			
			for (j = 0; j < Length; j++)
			{
				// test for loop signal
				if (chip->RAM[Addr] == 0xFF)
				{
					CH->Addr = (Addr = CH->Loop_Addr) << PCM_STEP_SHIFT;
					if (chip->RAM[Addr] == 0xFF)
						break;
					else
						j--;
				}
				else
				{
					if (chip->RAM[Addr] & 0x80)
					{
						CH->Data = chip->RAM[Addr] & 0x7F;
						bufL[j] -= CH->Data * CH->MUL_L;
						bufR[j] -= CH->Data * CH->MUL_R;
					}
					else
					{
						CH->Data = chip->RAM[Addr];
						bufL[j] += CH->Data * CH->MUL_L;
						bufR[j] += CH->Data * CH->MUL_R;
					}
					
					// update address register
					k = Addr + 1;
					CH->Addr = (CH->Addr + CH->Step) & 0x7FFFFFF;
					Addr = CH->Addr >> PCM_STEP_SHIFT;
					
					for (; k < Addr; k++)
					{
						if (chip->RAM[k] == 0xFF)
						{
							CH->Addr = (Addr = CH->Loop_Addr) << PCM_STEP_SHIFT;
							break;
						}
					}
				}
			}
			
			if (chip->RAM[Addr] == 0xFF)
			{
				CH->Addr = CH->Loop_Addr << PCM_STEP_SHIFT;
			}
		}
	}
#endif
	
	return 0;
}


void rf5c164_update(void *_chip, stream_sample_t **outputs, int samples)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	
	PCM_Update(chip, outputs, samples);
}

void * device_start_rf5c164( int clock )
{
	/* allocate memory for the chip */
	//rf5c164_state *chip = get_safe_token(device);
	struct pcm_chip_ *chip;
	int rate;
	
	chip = (struct pcm_chip_ *) malloc(sizeof(struct pcm_chip_));
	if (!chip) return chip;

	rate = clock / 384;
	
	PCM_Init(chip, rate);
	/* allocate the stream */
	//chip->stream = stream_create(device, 0, 2, device->clock / 384, chip, rf5c68_update);
	
	return chip;
}

void device_stop_rf5c164(void *_chip)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	free(chip->RAM);	chip->RAM = NULL;
	free(chip);
}

void device_reset_rf5c164(void *chip)
{
	//struct pcm_chip_ *chip = &PCM_Chip[ChipID];
	PCM_Reset(chip);
}

void rf5c164_w(void *chip, offs_t offset, UINT8 data)
{
	//struct pcm_chip_ *chip = &PCM_Chip[ChipID];
	PCM_Write_Reg(chip, offset, data);
}

void rf5c164_mem_w(void *_chip, offs_t offset, UINT8 data)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	chip->RAM[chip->Bank | offset] = data;
}

void rf5c164_write_ram(void *_chip, offs_t DataStart, offs_t DataLength, const UINT8* RAMData)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	
	if (DataStart >= chip->RAMSize)
		return;
	if (DataStart + DataLength > chip->RAMSize)
		DataLength = chip->RAMSize - DataStart;
	
	memcpy(chip->RAM + (chip->Bank | DataStart), RAMData, DataLength);
	
	return;
}


void rf5c164_set_mute_mask(void *_chip, UINT32 MuteMask)
{
	struct pcm_chip_ *chip = (struct pcm_chip_ *) _chip;
	unsigned char CurChn;
	
	for (CurChn = 0; CurChn < 8; CurChn ++)
		chip->Channel[CurChn].Muted = (MuteMask >> CurChn) & 0x01;
	
	return;
}
