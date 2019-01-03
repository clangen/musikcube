/***************************************************************************
 * Gens: PWM audio emulator.                                               *
 *                                                                         *
 * Copyright (c) 1999-2002 by Stéphane Dallongeville                       *
 * Copyright (c) 2003-2004 by Stéphane Akhoun                              *
 * Copyright (c) 2008 by David Korth                                       *
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

#define PWM_BUF_SIZE 4

#include "mamedef.h"

/*extern unsigned char PWM_FULL_TAB[PWM_BUF_SIZE * PWM_BUF_SIZE];

extern unsigned short PWM_FIFO_R[8];
extern unsigned short PWM_FIFO_L[8];
extern unsigned int PWM_RP_R;
extern unsigned int PWM_WP_R;
extern unsigned int PWM_RP_L;
extern unsigned int PWM_WP_L;
extern unsigned int PWM_Cycles;
extern unsigned int PWM_Cycle;
extern unsigned int PWM_Cycle_Cnt;
extern unsigned int PWM_Int;
extern unsigned int PWM_Int_Cnt;
extern unsigned int PWM_Mode;
extern unsigned int PWM_Enable;
extern unsigned int PWM_Out_R;
extern unsigned int PWM_Out_L;*/

//void PWM_Init(void);
//void PWM_Recalc_Scale(void);

/* Functions called by x86 asm. */
//void PWM_Set_Cycle(unsigned int cycle);
//void PWM_Set_Int(unsigned int int_time);

/* Functions called by C/C++ code only. */
//void PWM_Clear_Timer(void);
//void PWM_Update_Timer(unsigned int cycle);
//void PWM_Update(int **buf, int length);

#ifdef __cplusplus
extern "C" {
#endif

void pwm_update(void *chip, stream_sample_t **outputs, int samples);

void * device_start_pwm(int clock);
void device_stop_pwm(void *chip);
void device_reset_pwm(void *chip);

void pwm_chn_w(void *chip, UINT8 Channel, UINT16 data);

#ifdef __cplusplus
}
#endif
