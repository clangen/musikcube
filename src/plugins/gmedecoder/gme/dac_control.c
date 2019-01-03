 /************************
  *  DAC Stream Control  *
  ***********************/
// (Custom Driver to handle PCM Streams of YM2612 DAC and PWM.)
//
// Written on 3 February 2011 by Valley Bell
// Last Update: 25 April 2011
//
// Only for usage in non-commercial, VGM file related software.

/* How it basically works:

1. send command X with data Y at frequency F to chip C
2. do that until you receive a STOP command, or until you sent N commands

*/

#include "dac_control.h"

#include <stdlib.h>

#define INLINE static __inline

void chip_reg_write(void * context, UINT32 Sample, UINT8 ChipType, UINT8 ChipID, UINT8 Port, UINT8 Offset, UINT8 Data);

typedef struct _dac_control
{
	UINT32 SampleRate;

	// Commands sent to dest-chip
	UINT8 DstChipType;
	UINT8 DstChipID;
	UINT16 DstCommand;
	UINT8 CmdSize;
	
	UINT32 Frequency;	// Frequency (Hz) at which the commands are sent
	UINT32 DataLen;		// to protect from reading beyond End Of Data
	const UINT8* Data;
	UINT32 DataStart;	// Position where to start
	UINT8 StepSize;		// usually 1, set to 2 for L/R interleaved data
	UINT8 StepBase;		// usually 0, set to 0/1 for L/R interleaved data
	UINT32 CmdsToSend;
	
	// Running Bits:	0 (01) - is playing
	//					2 (04) - loop sample (simple loop from start to end)
	//					4 (10) - already sent this command
	//					7 (80) - disabled
	UINT8 Running;
	UINT32 Step;
	UINT32 Pos;
	UINT32 RemainCmds;
	UINT8 DataStep;		// always StepSize * CmdSize

	void * context; // context data sent to chip_reg_write
} dac_control;

#ifndef NULL
#define NULL	(void*)0
#endif

static void daccontrol_SendCommand(dac_control *chip, UINT32 Sample)
{
	UINT8 Port;
	UINT8 Command;
	UINT8 Data;
	const UINT8* ChipData;
	
	if (chip->Running & 0x10)	// command already sent
		return;
	if (chip->DataStart + chip->Pos >= chip->DataLen)
		return;
	
	ChipData = chip->Data + (chip->DataStart + chip->Pos);
	switch(chip->DstChipType)
	{
	// Support for the important chips
	case 0x02:	// YM2612
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, Port, Command, Data);
		break;
	case 0x11:	// PWM
		Port = (chip->DstCommand & 0x000F) >> 0;
		Command = ChipData[0x01] & 0x0F;
		Data = ChipData[0x00];
		chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, Port, Command, Data);
		break;
	// (Generic) Support for other chips (just for completeness)
	case 0x00:	// SN76496
		Command = (chip->DstCommand & 0x00F0) >> 0;
		Data = ChipData[0x00] & 0x0F;
		if (Command & 0x10)
		{
			// Volume Change (4-Bit value)
			chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, 0x00, 0x00, Command | Data);
		}
		else
		{
			// Frequency Write (10-Bit value)
			Port = ((ChipData[0x01] & 0x03) << 4) | ((ChipData[0x00] & 0xF0) >> 4);
			chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, 0x00, 0x00, Command | Data);
			chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, 0x00, 0x00, Port);
		}
		break;
	case 0x01:	// YM2413
	case 0x03:	// YM2151
	case 0x09:	// YM3812
	case 0x0A:	// YM3526
	case 0x0B:	// Y8950
	case 0x0F:	// YMZ280B
	case 0x12:	// AY8910
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, 0x00, Command, Data);
		break;
	case 0x06:	// YM2203
	case 0x07:	// YM2608
	case 0x08:	// YM2610/B
	case 0x0C:	// YMF262
	case 0x0D:	// YMF278B
	case 0x0E:	// YMF271
		Port = (chip->DstCommand & 0xFF00) >> 8;
		Command = (chip->DstCommand & 0x00FF) >> 0;
		Data = ChipData[0x00];
		chip_reg_write(chip->context, Sample, chip->DstChipType, chip->DstChipID, Port, Command, Data);
		break;
	}
	chip->Running |= 0x10;
	
	return;
}

INLINE UINT32 muldiv64round(UINT32 Multiplicand, UINT32 Multiplier, UINT32 Divisor)
{
	// Yes, I'm correctly rounding the values.
	return (UINT32)(((UINT64)Multiplicand * Multiplier + Multiplier / 2) / Divisor);
}

void daccontrol_update(void *_chip, UINT32 base_clock, UINT32 samples)
{
	dac_control *chip = (dac_control *) _chip;
	UINT32 NewPos;
	UINT32 Sample;
	
	if (chip->Running & 0x80)	// disabled
		return;
	if (! (chip->Running & 0x01))	// stopped
		return;
	
	/*if (samples > 0x20)
	{
		// very effective Speed Hack for fast seeking
		NewPos = chip->Step + (samples - 0x10);
		NewPos = muldiv64round(NewPos * chip->DataStep, chip->Frequency, chip->SampleRate);
		while(chip->RemainCmds && chip->Pos < NewPos)
		{
			chip->Pos += chip->DataStep;
			chip->RemainCmds --;
		}
	}*/
	
	Sample = 0;
	chip->Step += samples;
	// Formula: Step * Freq / SampleRate
	NewPos = muldiv64round(chip->Step * chip->DataStep, chip->Frequency, chip->SampleRate);
	
	while(chip->RemainCmds && chip->Pos < NewPos)
	{
		daccontrol_SendCommand(chip, base_clock + muldiv64round(Sample, chip->SampleRate, chip->Frequency));
		Sample++;
		chip->Pos += chip->DataStep;
		chip->Running &= ~0x10;
		chip->RemainCmds --;
	}
	
	if (! chip->RemainCmds && (chip->Running & 0x04))
	{
		// loop back to start
		chip->RemainCmds = chip->CmdsToSend;
		chip->Step = 0x00;
		chip->Pos = 0x00;
	}
	
	if (! chip->RemainCmds)
		chip->Running &= ~0x01;	// stop
	
	return;
}

void * device_start_daccontrol(UINT32 samplerate, void * context)
{
	dac_control *chip;
	
	chip = (dac_control *) calloc(1, sizeof(dac_control));

	chip->SampleRate = samplerate;
	chip->context = context;
	
	chip->DstChipType = 0xFF;
	chip->DstChipID = 0x00;
	chip->DstCommand = 0x0000;
	
	chip->Running = 0xFF;	// disable all actions (except setup_chip)
	
	return chip;
}

void device_stop_daccontrol(void *_chip)
{
	dac_control *chip = (dac_control *) _chip;
	
	free( chip );
}

void device_reset_daccontrol(void *_chip)
{
	dac_control *chip = (dac_control *) _chip;
	
	chip->DstChipType = 0x00;
	chip->DstChipID = 0x00;
	chip->DstCommand = 0x00;
	chip->CmdSize = 0x00;
	
	chip->Frequency = 0;
	chip->DataLen = 0x00;
	chip->Data = NULL;
	chip->DataStart = 0x00;
	chip->StepSize = 0x00;
	chip->StepBase = 0x00;
	
	chip->Running = 0x00;
	chip->Step = 0x00;
	chip->Pos = 0x00;
	chip->RemainCmds = 0x00;
	chip->DataStep = 0x00;
	
	return;
}

void daccontrol_setup_chip(void *_chip, UINT8 ChType, UINT8 ChNum, UINT16 Command)
{
	dac_control *chip = (dac_control *) _chip;
	
	chip->DstChipType = ChType;	// TypeID (e.g. 0x02 for YM2612)
	chip->DstChipID = ChNum;	// chip number (to send commands to 1st or 2nd chip)
	chip->DstCommand = Command;	// Port and Command (would be 0x02A for YM2612)
	
	switch(chip->DstChipType)
	{
	case 0x00:	// SN76496
		if (chip->DstCommand & 0x0010)
			chip->CmdSize = 0x01;	// Volume Write
		else
			chip->CmdSize = 0x02;	// Frequency Write
		break;
	case 0x02:	// YM2612
		chip->CmdSize = 0x01;
		break;
	case 0x11:	// PWM
		chip->CmdSize = 0x02;
		break;
	default:
		chip->CmdSize = 0x01;
		break;
	}
	chip->DataStep = chip->CmdSize * chip->StepSize;
	
	return;
}

void daccontrol_set_data(void *_chip, const UINT8* Data, UINT32 DataLen, UINT8 StepSize, UINT8 StepBase)
{
	dac_control *chip = (dac_control *) _chip;
	
	if (chip->Running & 0x80)
		return;
	
	if (DataLen && Data != NULL)
	{
		chip->DataLen = DataLen;
		chip->Data = Data;
	}
	else
	{
		chip->DataLen = 0x00;
		chip->Data = NULL;
	}
	chip->StepSize = StepSize ? StepSize : 1;
	chip->StepBase = StepBase;
	chip->DataStep = chip->CmdSize * chip->StepSize;
	
	return;
}

void daccontrol_set_frequency(void *_chip, UINT32 Frequency)
{
	dac_control *chip = (dac_control *) _chip;
	
	if (chip->Running & 0x80)
		return;
	
	chip->Frequency = Frequency;
	
	return;
}

void daccontrol_start(void *_chip, UINT32 DataPos, UINT8 LenMode, UINT32 Length)
{
	dac_control *chip = (dac_control *) _chip;
	UINT16 CmdStepBase;
	
	if (chip->Running & 0x80)
		return;
	
	CmdStepBase = chip->CmdSize * chip->StepBase;
	if (DataPos != 0xFFFFFFFF)	// skip setting DataStart, if Pos == -1
	{
		chip->DataStart = DataPos + CmdStepBase;
		if (chip->DataStart > chip->DataLen)	// catch bad value and force silence
			chip->DataStart = chip->DataLen;
	}
	
	switch(LenMode & 0x0F)
	{
	case DCTRL_LMODE_IGNORE:	// Length is already set - ignore
		break;
	case DCTRL_LMODE_CMDS:		// Length = number of commands
		chip->CmdsToSend = Length;
		break;
	case DCTRL_LMODE_MSEC:		// Length = time in msec
		chip->CmdsToSend = 1000 * Length / chip->Frequency;
		break;
	case DCTRL_LMODE_TOEND:		// play unti stop-command is received (or data-end is reached)
		chip->CmdsToSend = (chip->DataLen - (chip->DataStart - CmdStepBase)) / chip->DataStep;
		break;
	case DCTRL_LMODE_BYTES:		// raw byte count
		chip->CmdsToSend = Length / chip->DataStep;
		break;
	default:
		chip->CmdsToSend = 0x00;
		break;
	}
	chip->RemainCmds = chip->CmdsToSend;
	chip->Step = 0x00;
	chip->Pos = 0x00;
	
	chip->Running &= ~0x04;
	chip->Running |= (LenMode & 0x80) ? 0x04 : 0x00;	// set loop mode
	
	chip->Running |= 0x01;	// start
	chip->Running &= ~0x10;	// command isn't yet sent
	
	return;
}

void daccontrol_stop(void *_chip)
{
	dac_control *chip = (dac_control *) _chip;
	
	if (chip->Running & 0x80)
		return;
	
	chip->Running &= ~0x01;	// stop
	
	return;
}
