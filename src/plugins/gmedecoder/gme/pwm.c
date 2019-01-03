/***************************************************************************
 * Gens: PWM audio emulator.                                               *
 *                                                                         *
 * Copyright (c) 1999-2002 by Stéphane Dallongeville                       *
 * Copyright (c) 2003-2004 by Stéphane Akhoun                              *
 * Copyright (c) 2008-2009 by David Korth                                  *
 *                                                                         *
 * This program is free software; you can redistribute it and/or modify it *
 * under the terms of the GNU General Public License as published by the   *
 * Free Software Foundation; either version 2 of the License, or (at your  *
 * option) any later version.                                              *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License along *
 * with this program; if not, write to the Free Software Foundation, Inc., *
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.           *
 ***************************************************************************/

#include "pwm.h"

#include <string.h>
#include <stdlib.h>

#ifndef INLINE
#define INLINE static __inline
#endif

//#include "gens_core/mem/mem_sh2.h"
//#include "gens_core/cpu/sh2/sh2.h"

#define CHILLY_WILLY_SCALE	1

#if PWM_BUF_SIZE == 8
unsigned char PWM_FULL_TAB[PWM_BUF_SIZE * PWM_BUF_SIZE] =
{
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
	0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x80, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40,
};
#elif PWM_BUF_SIZE == 4
unsigned char PWM_FULL_TAB[PWM_BUF_SIZE * PWM_BUF_SIZE] =
{
	0x40, 0x00, 0x00, 0x80,
	0x80, 0x40, 0x00, 0x00,
	0x00, 0x80, 0x40, 0x00,
	0x00, 0x00, 0x80, 0x40,
};
#else
#error PWM_BUF_SIZE must equal 4 or 8.
#endif /* PWM_BUF_SIZE */

typedef struct _pwm_chip
{
	unsigned short PWM_FIFO_R[8];
	unsigned short PWM_FIFO_L[8];
	unsigned int PWM_RP_R;
	unsigned int PWM_WP_R;
	unsigned int PWM_RP_L;
	unsigned int PWM_WP_L;
	unsigned int PWM_Cycles;
	unsigned int PWM_Cycle;
	unsigned int PWM_Cycle_Cnt;
	unsigned int PWM_Int;
	unsigned int PWM_Int_Cnt;
	unsigned int PWM_Mode;
	//unsigned int PWM_Enable;
	unsigned int PWM_Out_R;
	unsigned int PWM_Out_L;

	unsigned int PWM_Cycle_Tmp;
	unsigned int PWM_Cycles_Tmp;
	unsigned int PWM_Int_Tmp;
	unsigned int PWM_FIFO_L_Tmp;
	unsigned int PWM_FIFO_R_Tmp;

#if CHILLY_WILLY_SCALE
// TODO: Fix Chilly Willy's new scaling algorithm.
	/* PWM scaling variables. */
	int PWM_Offset;
	int PWM_Scale;
	//int PWM_Loudness;
#endif
	
	int clock;
} pwm_chip;
#if CHILLY_WILLY_SCALE
// TODO: Fix Chilly Willy's new scaling algorithm.
#define PWM_Loudness	0
#endif

void PWM_Init(pwm_chip* chip);
void PWM_Recalc_Scale(pwm_chip* chip);

void PWM_Set_Cycle(pwm_chip* chip, unsigned int cycle);
void PWM_Set_Int(pwm_chip* chip, unsigned int int_time);

void PWM_Update(pwm_chip* chip, int **buf, int length);


/**
 * PWM_Init(): Initialize the PWM audio emulator.
 */
void PWM_Init(pwm_chip* chip)
{
	chip->PWM_Mode = 0;
	chip->PWM_Out_R = 0;
	chip->PWM_Out_L = 0;
	
	memset(chip->PWM_FIFO_R, 0x00, sizeof(chip->PWM_FIFO_R));
	memset(chip->PWM_FIFO_L, 0x00, sizeof(chip->PWM_FIFO_L));
	
	chip->PWM_RP_R = 0;
	chip->PWM_WP_R = 0;
	chip->PWM_RP_L = 0;
	chip->PWM_WP_L = 0;
	chip->PWM_Cycle_Tmp = 0;
	chip->PWM_Int_Tmp = 0;
	chip->PWM_FIFO_L_Tmp = 0;
	chip->PWM_FIFO_R_Tmp = 0;
	
	//PWM_Loudness = 0;
	PWM_Set_Cycle(chip, 0);
	PWM_Set_Int(chip, 0);
}


#if CHILLY_WILLY_SCALE
// TODO: Fix Chilly Willy's new scaling algorithm.
void PWM_Recalc_Scale(pwm_chip* chip)
{
	chip->PWM_Offset = (chip->PWM_Cycle / 2) + 1;
	chip->PWM_Scale = 0x7FFF00 / chip->PWM_Offset;
}
#endif


void PWM_Set_Cycle(pwm_chip* chip, unsigned int cycle)
{
	cycle--;
	chip->PWM_Cycle = (cycle & 0xFFF);
	chip->PWM_Cycle_Cnt = chip->PWM_Cycles;
	
#if CHILLY_WILLY_SCALE
	// TODO: Fix Chilly Willy's new scaling algorithm.
	PWM_Recalc_Scale(chip);
#endif
}


void PWM_Set_Int(pwm_chip* chip, unsigned int int_time)
{
	int_time &= 0x0F;
	if (int_time)
		chip->PWM_Int = chip->PWM_Int_Cnt = int_time;
	else
		chip->PWM_Int = chip->PWM_Int_Cnt = 16;
}


void PWM_Clear_Timer(pwm_chip* chip)
{
	chip->PWM_Cycle_Cnt = 0;
}


/**
 * PWM_SHIFT(): Shift PWM data.
 * @param src: Channel (L or R) with the source data.
 * @param dest Channel (L or R) for the destination.
 */
#define PWM_SHIFT(src, dest)										\
{													\
	/* Make sure the source FIFO isn't empty. */							\
	if (PWM_RP_##src != PWM_WP_##src)								\
	{												\
		/* Get destination channel output from the source channel FIFO. */			\
		PWM_Out_##dest = PWM_FIFO_##src[PWM_RP_##src];						\
													\
		/* Increment the source channel read pointer, resetting to 0 if it overflows. */	\
		PWM_RP_##src = (PWM_RP_##src + 1) & (PWM_BUF_SIZE - 1);					\
	}												\
}


/*static void PWM_Shift_Data(void)
{
	switch (PWM_Mode & 0x0F)
	{
		case 0x01:
		case 0x0D:
			// Rx_LL: Right -> Ignore, Left -> Left
			PWM_SHIFT(L, L);
			break;
		
		case 0x02:
		case 0x0E:
			// Rx_LR: Right -> Ignore, Left -> Right
			PWM_SHIFT(L, R);
			break;
		
		case 0x04:
		case 0x07:
			// RL_Lx: Right -> Left, Left -> Ignore
			PWM_SHIFT(R, L);
			break;
		
		case 0x05:
		case 0x09:
			// RR_LL: Right -> Right, Left -> Left
			PWM_SHIFT(L, L);
			PWM_SHIFT(R, R);
			break;
		
		case 0x06:
		case 0x0A:
			// RL_LR: Right -> Left, Left -> Right
			PWM_SHIFT(L, R);
			PWM_SHIFT(R, L);
			break;
		
		case 0x08:
		case 0x0B:
			// RR_Lx: Right -> Right, Left -> Ignore
			PWM_SHIFT(R, R);
			break;
		
		case 0x00:
		case 0x03:
		case 0x0C:
		case 0x0F:
		default:
			// Rx_Lx: Right -> Ignore, Left -> Ignore
			break;
	}
}


void PWM_Update_Timer(unsigned int cycle)
{
	// Don't do anything if PWM is disabled in the Sound menu.
	
	// Don't do anything if PWM isn't active.
	if ((PWM_Mode & 0x0F) == 0x00)
		return;
	
	if (PWM_Cycle == 0x00 || (PWM_Cycle_Cnt > cycle))
		return;
	
	PWM_Shift_Data();
	
	PWM_Cycle_Cnt += PWM_Cycle;
	
	PWM_Int_Cnt--;
	if (PWM_Int_Cnt == 0)
	{
		PWM_Int_Cnt = PWM_Int;
		
		if (PWM_Mode & 0x0080)
		{
			// RPT => generate DREQ1 as well as INT
			SH2_DMA1_Request(&M_SH2, 1);
			SH2_DMA1_Request(&S_SH2, 1);
		}
		
		if (_32X_MINT & 1)
			SH2_Interrupt(&M_SH2, 6);
		if (_32X_SINT & 1)
			SH2_Interrupt(&S_SH2, 6);
	}
}*/


INLINE int PWM_Update_Scale(pwm_chip* chip, int PWM_In)
{
	if (PWM_In == 0)
		return 0;
	
	// TODO: Chilly Willy's new scaling algorithm breaks drx's Sonic 1 32X (with PWM drums).
#ifdef CHILLY_WILLY_SCALE
	//return (((PWM_In & 0xFFF) - chip->PWM_Offset) * chip->PWM_Scale) >> (8 - PWM_Loudness);
	// Knuckles' Chaotix: Tachy Touch uses the values 0xF?? for negative values
	// This small modification fixes the terrible pops.
	PWM_In &= 0xFFF;
	if (PWM_In & 0x800)
		PWM_In |= ~0xFFF;
	return ((PWM_In - chip->PWM_Offset) * chip->PWM_Scale) >> (8 - PWM_Loudness);
#else
	const int PWM_adjust = ((chip->PWM_Cycle >> 1) + 1);
	int PWM_Ret = ((chip->PWM_In & 0xFFF) - PWM_adjust);
	
	// Increase PWM volume so it's audible.
	PWM_Ret <<= (5+2);
	
	// Make sure the PWM isn't oversaturated.
	if (PWM_Ret > 32767)
		PWM_Ret = 32767;
	else if (PWM_Ret < -32768)
		PWM_Ret = -32768;
	
	return PWM_Ret;
#endif
}


void PWM_Update(pwm_chip* chip, int **buf, int length)
{
	int tmpOutL;
	int tmpOutR;
	int i;
	
	//if (!PWM_Enable)
	//	return;
	
	if (chip->PWM_Out_L == 0 && chip->PWM_Out_R == 0)
	{
		memset(buf[0], 0x00, length * sizeof(int));
		memset(buf[1], 0x00, length * sizeof(int));
		return;
	}
	
	// New PWM scaling algorithm provided by Chilly Willy on the Sonic Retro forums.
	tmpOutL = PWM_Update_Scale(chip, (int)chip->PWM_Out_L);
	tmpOutR = PWM_Update_Scale(chip, (int)chip->PWM_Out_R);
	
	for (i = 0; i < length; i ++)
	{
		buf[0][i] = tmpOutL;
		buf[1][i] = tmpOutR;
	}
}


void pwm_update(void *_chip, stream_sample_t **outputs, int samples)
{
	pwm_chip *chip = (pwm_chip *) _chip;
	
	PWM_Update(chip, outputs, samples);
}

void * device_start_pwm(int clock)
{
	/* allocate memory for the chip */
	//pwm_state *chip = get_safe_token(device);
	pwm_chip *chip;
	int rate;
	
	chip = (pwm_chip *) malloc(sizeof(pwm_chip));
	if (!chip) return chip;

	rate = 22020;	// that's the rate the PWM is mostly used
	chip->clock = clock;
	
	PWM_Init(chip);
	/* allocate the stream */
	//chip->stream = stream_create(device, 0, 2, device->clock / 384, chip, rf5c68_update);
	
	return chip;
}

void device_stop_pwm(void *chip)
{
	//pwm_chip *chip = &PWM_Chip[ChipID];
	//free(chip->ram);
	free(chip);
}

void device_reset_pwm(void *_chip)
{
	pwm_chip *chip = (pwm_chip *) _chip;
	PWM_Init(chip);
}

void pwm_chn_w(void *_chip, UINT8 Channel, UINT16 data)
{
	pwm_chip *chip = (pwm_chip *) _chip;
	
	if (chip->clock == 1)
	{	// old-style commands
		switch(Channel)
		{
		case 0x00:
			chip->PWM_Out_L = data;
			break;
		case 0x01:
			chip->PWM_Out_R = data;
			break;
		case 0x02:
			PWM_Set_Cycle(chip, data);
			break;
		case 0x03:
			chip->PWM_Out_L = data;
			chip->PWM_Out_R = data;
			break;
		}
	}
	else
	{
		switch(Channel)
		{
		case 0x00/2:	// control register
			PWM_Set_Int(chip, data >> 8);
			break;
		case 0x02/2:	// cycle register
			PWM_Set_Cycle(chip, data);
			break;
		case 0x04/2:	// l ch
			chip->PWM_Out_L = data;
			break;
		case 0x06/2:	// r ch
			chip->PWM_Out_R = data;
			if (! chip->PWM_Mode)
			{
				if (chip->PWM_Out_L == chip->PWM_Out_R)
				{
					// fixes these terrible pops when
					// starting/stopping/pausing the song
					chip->PWM_Offset = data;
					chip->PWM_Mode = 0x01;
				}
			}
			break;
		case 0x08/2:	// mono ch
			chip->PWM_Out_L = data;
			chip->PWM_Out_R = data;
			if (! chip->PWM_Mode)
			{
				chip->PWM_Offset = data;
				chip->PWM_Mode = 0x01;
			}
			break;
		}
	}
	
	return;
}
