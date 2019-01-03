// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Vgm_Core.h"

#include "dac_control.h"

#include "blargg_endian.h"
#include <math.h>

// Needed for OKIM6295 system detection
#include "Vgm_Emu.h"

/* Copyright (C) 2003-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

int const stereo         =  2;
int const fm_time_bits   = 12;
int const blip_time_bits = 12;

enum {
	cmd_gg_stereo       = 0x4F,
	cmd_gg_stereo_2     = 0x3F,
	cmd_psg             = 0x50,
	cmd_psg_2           = 0x30,
	cmd_ym2413          = 0x51,
	cmd_ym2413_2        = 0xA1,
	cmd_ym2612_port0    = 0x52,
	cmd_ym2612_2_port0  = 0xA2,
	cmd_ym2612_port1    = 0x53,
	cmd_ym2612_2_port1  = 0xA3,
	cmd_ym2151          = 0x54,
	cmd_ym2151_2        = 0xA4,
	cmd_ym2203          = 0x55,
	cmd_ym2203_2        = 0xA5,
	cmd_ym2608_port0    = 0x56,
	cmd_ym2608_2_port0  = 0xA6,
	cmd_ym2608_port1    = 0x57,
	cmd_ym2608_2_port1  = 0xA7,
	cmd_ym2610_port0    = 0x58,
	cmd_ym2610_2_port0  = 0xA8,
	cmd_ym2610_port1    = 0x59,
	cmd_ym2610_2_port1  = 0xA9,
	cmd_ym3812          = 0x5A,
	cmd_ym3812_2        = 0xAA,
	cmd_ymz280b         = 0x5D,
	cmd_ymf262_port0    = 0x5E,
	cmd_ymf262_2_port0  = 0xAE,
	cmd_ymf262_port1    = 0x5F,
	cmd_ymf262_2_port1  = 0xAF,
	cmd_delay           = 0x61,
	cmd_delay_735       = 0x62,
	cmd_delay_882       = 0x63,
	cmd_byte_delay      = 0x64,
	cmd_end             = 0x66,
	cmd_data_block      = 0x67,
	cmd_ram_block       = 0x68,
	cmd_short_delay     = 0x70,
	cmd_pcm_delay       = 0x80,
	cmd_dacctl_setup    = 0x90,
	cmd_dacctl_data     = 0x91,
	cmd_dacctl_freq     = 0x92,
	cmd_dacctl_play     = 0x93,
	cmd_dacctl_stop     = 0x94,
	cmd_dacctl_playblock= 0x95,
	cmd_ay8910          = 0xA0,
	cmd_rf5c68          = 0xB0,
	cmd_rf5c164         = 0xB1,
	cmd_pwm             = 0xB2,
	cmd_gbdmg_write     = 0xB3,
	cmd_okim6258_write  = 0xB7,
	cmd_okim6295_write  = 0xB8,
    cmd_huc6280_write   = 0xB9,
	cmd_k053260_write   = 0xBA,
	cmd_segapcm_write   = 0xC0,
	cmd_rf5c68_mem      = 0xC1,
	cmd_rf5c164_mem     = 0xC2,
    cmd_qsound_write    = 0xC4,
	cmd_k051649_write   = 0xD2,
	cmd_k054539_write   = 0xD3,
	cmd_c140            = 0xD4,
	cmd_pcm_seek        = 0xE0,

	rf5c68_ram_block    = 0x01,
	rf5c164_ram_block   = 0x02,
	
	pcm_block_type      = 0x00,
	pcm_aux_block_type  = 0x40,
	rom_block_type      = 0x80,
	ram_block_type      = 0xC0,

	rom_segapcm         = 0x80,
	rom_ym2608_deltat   = 0x81,
	rom_ym2610_adpcm    = 0x82,
	rom_ym2610_deltat   = 0x83,
	rom_ymz280b         = 0x86,
	rom_okim6295        = 0x8B,
	rom_k054539         = 0x8C,
	rom_c140            = 0x8D,
	rom_k053260         = 0x8E,
    rom_qsound          = 0x8F,

	ram_rf5c68          = 0xC0,
	ram_rf5c164         = 0xC1,
	ram_nesapu          = 0xC2,

	ym2612_dac_port     = 0x2A,
	ym2612_dac_pan_port = 0xB6
};

inline int command_len( int command )
{
	static byte const lens [0x10] = {
	// 0 1 2 3 4 5 6 7 8 9 A B C D E F
	   1,1,1,2,2,3,1,1,1,1,3,3,4,4,5,5
	};
	int len = lens [command >> 4];
	check( len != 1 );
	return len;
}

int Vgm_Core::run_ym2151( int chip, int time )
{
	return ym2151[!!chip].run_until( time );
}

int Vgm_Core::run_ym2203( int chip, int time )
{
	return ym2203[!!chip].run_until( time );
}

int Vgm_Core::run_ym2413( int chip, int time )
{
	return ym2413[!!chip].run_until( time );
}

int Vgm_Core::run_ym2612( int chip, int time )
{
	return ym2612[!!chip].run_until( time );
}

int Vgm_Core::run_ym2610( int chip, int time )
{
	return ym2610[!!chip].run_until( time );
}

int Vgm_Core::run_ym2608( int chip, int time )
{
	return ym2608[!!chip].run_until( time );
}

int Vgm_Core::run_ym3812( int chip, int time )
{
	return ym3812[!!chip].run_until( time );
}

int Vgm_Core::run_ymf262( int chip, int time )
{
	return ymf262[!!chip].run_until( time );
}

int Vgm_Core::run_ymz280b( int time )
{
	return ymz280b.run_until( time );
}

int Vgm_Core::run_c140( int time )
{
	return c140.run_until( time );
}

int Vgm_Core::run_segapcm( int time )
{
	return segapcm.run_until( time );
}

int Vgm_Core::run_rf5c68( int time )
{
	return rf5c68.run_until( time );
}

int Vgm_Core::run_rf5c164( int time )
{
	return rf5c164.run_until( time );
}

int Vgm_Core::run_pwm( int time )
{
	return pwm.run_until( time );
}

int Vgm_Core::run_okim6258( int chip, int time )
{
    chip = !!chip;
    if ( okim6258[chip].enabled() )
    {
        int current_clock = okim6258[chip].get_clock();
        if ( okim6258_hz[chip] != current_clock )
        {
            okim6258_hz[chip] = current_clock;
            okim6258[chip].setup( (double)okim6258_hz[chip] / vgm_rate, 0.85, 1.0 );
        }
    }
	return okim6258[chip].run_until( time );
}

int Vgm_Core::run_okim6295( int chip, int time )
{
	return okim6295[!!chip].run_until( time );
}

int Vgm_Core::run_k051649( int time )
{
	return k051649.run_until( time );
}

int Vgm_Core::run_k053260( int time )
{
	return k053260.run_until( time );
}

int Vgm_Core::run_k054539( int time )
{
	return k054539.run_until( time );
}

int Vgm_Core::run_qsound( int chip, int time )
{
    return qsound[!!chip].run_until( time );
}

/* Recursive fun starts here! */
int Vgm_Core::run_dac_control( int time )
{
	if (dac_control_recursion) return 1;

	++dac_control_recursion;
	for ( unsigned i = 0; i < DacCtrlUsed; i++ )
	{
		int time_start = DacCtrlTime[DacCtrlMap[i]];
		if ( time > time_start )
		{
			DacCtrlTime[DacCtrlMap[i]] = time;
			daccontrol_update( dac_control [i], time_start, time - time_start );
		}
	}
	--dac_control_recursion;

	return 1;
}

Vgm_Core::Vgm_Core()
{
	blip_buf[0] = stereo_buf[0].center();
	blip_buf[1] = blip_buf[0];
	has_looped = false;
	DacCtrlUsed = 0;
	dac_control = NULL;
	memset( PCMBank, 0, sizeof( PCMBank ) );
	memset( &PCMTbl, 0, sizeof( PCMTbl ) );
	memset( DacCtrl, 0, sizeof( DacCtrl ) );
	memset( DacCtrlTime, 0, sizeof( DacCtrlTime ) );
}

Vgm_Core::~Vgm_Core()
{
	for (unsigned i = 0; i < DacCtrlUsed; i++) device_stop_daccontrol( dac_control [i] );
	if ( dac_control ) free( dac_control );
	for (unsigned i = 0; i < PCM_BANK_COUNT; i++)
	{
		if ( PCMBank [i].Bank ) free( PCMBank [i].Bank );
		if ( PCMBank [i].Data ) free( PCMBank [i].Data );
	}
	if ( PCMTbl.Entries ) free( PCMTbl.Entries );
}

typedef unsigned int FUINT8;
typedef unsigned int FUINT16;

void Vgm_Core::ReadPCMTable(unsigned DataSize, const byte* Data)
{
	byte ValSize;
	unsigned TblSize;

	PCMTbl.ComprType = Data[0x00];
	PCMTbl.CmpSubType = Data[0x01];
	PCMTbl.BitDec = Data[0x02];
	PCMTbl.BitCmp = Data[0x03];
	PCMTbl.EntryCount = get_le16( Data + 0x04 );

	ValSize = (PCMTbl.BitDec + 7) / 8;
	TblSize = PCMTbl.EntryCount * ValSize;

	PCMTbl.Entries = realloc(PCMTbl.Entries, TblSize);
	memcpy(PCMTbl.Entries, Data + 0x06, TblSize);
}

bool Vgm_Core::DecompressDataBlk(VGM_PCM_DATA* Bank, unsigned DataSize, const byte* Data)
{
	UINT8 ComprType;
	UINT8 BitDec;
	FUINT8 BitCmp;
	UINT8 CmpSubType;
	UINT16 AddVal;
	const UINT8* InPos;
	const UINT8* InDataEnd;
	UINT8* OutPos;
	const UINT8* OutDataEnd;
	FUINT16 InVal;
	FUINT16 OutVal = 0;
	FUINT8 ValSize;
	FUINT8 InShift;
	FUINT8 OutShift;
	UINT8* Ent1B;
	UINT16* Ent2B;

	// ReadBits Variables
	FUINT8 BitsToRead;
	FUINT8 BitReadVal;
	FUINT8 InValB;
	FUINT8 BitMask;
	FUINT8 OutBit;

	// Variables for DPCM
	UINT16 OutMask;

	ComprType = Data[0x00];
	Bank->DataSize = get_le32( Data + 0x01 );

	switch(ComprType)
	{
	case 0x00:	// n-Bit compression
		BitDec = Data[0x05];
		BitCmp = Data[0x06];
		CmpSubType = Data[0x07];
		AddVal = get_le16( Data + 0x08 );

		if (CmpSubType == 0x02)
		{
			Ent1B = (UINT8*)PCMTbl.Entries;
			Ent2B = (UINT16*)PCMTbl.Entries;
			if (! PCMTbl.EntryCount)
			{
				Bank->DataSize = 0x00;
				return false;
			}
			else if (BitDec != PCMTbl.BitDec || BitCmp != PCMTbl.BitCmp)
			{
				Bank->DataSize = 0x00;
				return false;
			}
		}

		ValSize = (BitDec + 7) / 8;
		InPos = Data + 0x0A;
		InDataEnd = Data + DataSize;
		InShift = 0;
		OutShift = BitDec - BitCmp;
		OutDataEnd = Bank->Data + Bank->DataSize;

		for (OutPos = Bank->Data; OutPos < OutDataEnd && InPos < InDataEnd; OutPos += ValSize)
		{
			//InVal = ReadBits(Data, InPos, &InShift, BitCmp);
			// inlined - is 30% faster
			OutBit = 0x00;
			InVal = 0x0000;
			BitsToRead = BitCmp;
			while(BitsToRead)
			{
				BitReadVal = (BitsToRead >= 8) ? 8 : BitsToRead;
				BitsToRead -= BitReadVal;
				BitMask = (1 << BitReadVal) - 1;

				InShift += BitReadVal;
				InValB = (*InPos << InShift >> 8) & BitMask;
				if (InShift >= 8)
				{
					InShift -= 8;
					InPos ++;
					if (InShift)
						InValB |= (*InPos << InShift >> 8) & BitMask;
				}

				InVal |= InValB << OutBit;
				OutBit += BitReadVal;
			}

			switch(CmpSubType)
			{
			case 0x00:	// Copy
				OutVal = InVal + AddVal;
				break;
			case 0x01:	// Shift Left
				OutVal = (InVal << OutShift) + AddVal;
				break;
			case 0x02:	// Table
				switch(ValSize)
				{
				case 0x01:
					OutVal = Ent1B[InVal];
					break;
				case 0x02:
					OutVal = Ent2B[InVal];
					break;
				}
				break;
			}

			//memcpy(OutPos, &OutVal, ValSize);
			if (ValSize == 0x01)
				*((UINT8*)OutPos) = (UINT8)OutVal;
			else //if (ValSize == 0x02)
				*((UINT16*)OutPos) = (UINT16)OutVal;
		}
		break;
	case 0x01:	// Delta-PCM
		BitDec = Data[0x05];
		BitCmp = Data[0x06];
		OutVal = get_le16( Data + 0x08 );

		Ent1B = (UINT8*)PCMTbl.Entries;
		Ent2B = (UINT16*)PCMTbl.Entries;
		if (! PCMTbl.EntryCount)
		{
			Bank->DataSize = 0x00;
			return false;
		}
		else if (BitDec != PCMTbl.BitDec || BitCmp != PCMTbl.BitCmp)
		{
			Bank->DataSize = 0x00;
			return false;
		}

		ValSize = (BitDec + 7) / 8;
		OutMask = (1 << BitDec) - 1;
		InPos = Data + 0x0A;
		InDataEnd = Data + DataSize;
		InShift = 0;
		OutShift = BitDec - BitCmp;
		OutDataEnd = Bank->Data + Bank->DataSize;
		AddVal = 0x0000;

		for (OutPos = Bank->Data; OutPos < OutDataEnd && InPos < InDataEnd; OutPos += ValSize)
		{
			//InVal = ReadBits(Data, InPos, &InShift, BitCmp);
			// inlined - is 30% faster
			OutBit = 0x00;
			InVal = 0x0000;
			BitsToRead = BitCmp;
			while(BitsToRead)
			{
				BitReadVal = (BitsToRead >= 8) ? 8 : BitsToRead;
				BitsToRead -= BitReadVal;
				BitMask = (1 << BitReadVal) - 1;

				InShift += BitReadVal;
				InValB = (*InPos << InShift >> 8) & BitMask;
				if (InShift >= 8)
				{
					InShift -= 8;
					InPos ++;
					if (InShift)
						InValB |= (*InPos << InShift >> 8) & BitMask;
				}

				InVal |= InValB << OutBit;
				OutBit += BitReadVal;
			}

			switch(ValSize)
			{
			case 0x01:
				AddVal = Ent1B[InVal];
				OutVal += AddVal;
				OutVal &= OutMask;
				*((UINT8*)OutPos) = (UINT8)OutVal;
				break;
			case 0x02:
				AddVal = Ent2B[InVal];
				OutVal += AddVal;
				OutVal &= OutMask;
				*((UINT16*)OutPos) = (UINT16)OutVal;
				break;
			}
		}
		break;
	default:
		return false;
	}

	return true;
}

void Vgm_Core::AddPCMData(byte Type, unsigned DataSize, const byte* Data)
{
	unsigned CurBnk;
	VGM_PCM_BANK* TempPCM;
	VGM_PCM_DATA* TempBnk;
	unsigned BankSize;
	bool RetVal;


	if ((Type & 0x3F) >= PCM_BANK_COUNT || has_looped)
		return;

	if (Type == 0x7F)
	{
		ReadPCMTable( DataSize, Data );
		return;
	}

	TempPCM = &PCMBank[Type & 0x3F];
	CurBnk = TempPCM->BankCount;
	TempPCM->BankCount ++;
	TempPCM->BnkPos ++;
	if (TempPCM->BnkPos < TempPCM->BankCount)
		return;	// Speed hack (for restarting playback)
	TempPCM->Bank = (VGM_PCM_DATA*)realloc(TempPCM->Bank,
		sizeof(VGM_PCM_DATA) * TempPCM->BankCount);

	if (! (Type & 0x40))
		BankSize = DataSize;
	else
		BankSize = get_le32( Data + 1 );
	TempPCM->Data = ( byte * ) realloc(TempPCM->Data, TempPCM->DataSize + BankSize);
	TempBnk = &TempPCM->Bank[CurBnk];
	TempBnk->DataStart = TempPCM->DataSize;
	if (! (Type & 0x40))
	{
		TempBnk->DataSize = DataSize;
		TempBnk->Data = TempPCM->Data + TempBnk->DataStart;
		memcpy(TempBnk->Data, Data, DataSize);
	}
	else
	{
		TempBnk->Data = TempPCM->Data + TempBnk->DataStart;
		RetVal = DecompressDataBlk(TempBnk, DataSize, Data);
		if (! RetVal)
		{
			TempBnk->Data = NULL;
			TempBnk->DataSize = 0x00;
			return;
		}
	}
	TempPCM->DataSize += BankSize;
}

const byte* Vgm_Core::GetPointerFromPCMBank(byte Type, unsigned DataPos)
{
	if (Type >= PCM_BANK_COUNT)
		return NULL;

	if (DataPos >= PCMBank[Type].DataSize)
		return NULL;

	return &PCMBank[Type].Data[DataPos];
}

void Vgm_Core::dac_control_grow(byte chip_id)
{
	for ( unsigned i = 0; i < DacCtrlUsed; i++ )
	{
		if ( DacCtrlUsg [i] == chip_id )
		{
			device_reset_daccontrol( dac_control [i] );
			return;
		}
	}
	unsigned chip_mapped = DacCtrlUsed;
	DacCtrlUsg [DacCtrlUsed++] = chip_id;
	DacCtrlMap [chip_id] = chip_mapped;
	dac_control = (void**) realloc( dac_control, DacCtrlUsed * sizeof(void*) );
	dac_control [chip_mapped] = device_start_daccontrol( vgm_rate, this );
	device_reset_daccontrol( dac_control [chip_mapped] );
}

extern "C" void chip_reg_write(void * context, UINT32 Sample, UINT8 ChipType, UINT8 ChipID, UINT8 Port, UINT8 Offset, UINT8 Data)
{
	Vgm_Core * core = (Vgm_Core *) context;
	core->chip_reg_write(Sample, ChipType, ChipID, Port, Offset, Data);
}

void Vgm_Core::chip_reg_write(unsigned Sample, byte ChipType, byte ChipID, byte Port, byte Offset, byte Data)
{
	run_dac_control( Sample ); /* Let's get recursive! */
	ChipID = !!ChipID;
	switch (ChipType)
	{
	case 0x02:
		switch (Port)
		{
		case 0:
			if ( Offset == ym2612_dac_port )
			{
				write_pcm( Sample, ChipID, Data );
			}
			else if ( run_ym2612( ChipID, to_fm_time( Sample ) ) )
			{
				if ( Offset == 0x2B )
				{
					dac_disabled[ChipID] = (Data >> 7 & 1) - 1;
					dac_amp[ChipID] |= dac_disabled[ChipID];
				}
				ym2612[ChipID].write0( Offset, Data );
			}
			break;
		
		case 1:
			if ( run_ym2612( ChipID, to_fm_time( Sample ) ) )
			{
				if ( Offset == ym2612_dac_pan_port )
				{
					Blip_Buffer * blip_buf = NULL;
					switch ( Data >> 6 )
					{
					case 0: blip_buf = NULL; break;
					case 1: blip_buf = stereo_buf[0].right(); break;
					case 2: blip_buf = stereo_buf[0].left(); break;
					case 3: blip_buf = stereo_buf[0].center(); break;
					}
					/*if ( this->blip_buf != blip_buf )
					{
						blip_time_t blip_time = to_psg_time( vgm_time );
						if ( this->blip_buf ) pcm.offset_inline( blip_time, -dac_amp, this->blip_buf );
						if ( blip_buf )       pcm.offset_inline( blip_time,  dac_amp, blip_buf );
					}*/
					this->blip_buf[ChipID] = blip_buf;
				}
				ym2612[ChipID].write1( Offset, Data );
			}
			break;
		}
		break;

	case 0x11:
		if ( run_pwm( to_fm_time( Sample ) ) )
			pwm.write( Port, ( ( Offset ) << 8 ) + Data );
		break;

	case 0x00:
		psg[ChipID].write_data( to_psg_time( Sample ), Data );
		break;

	case 0x01:
		if ( run_ym2413( ChipID, to_fm_time( Sample ) ) )
			ym2413[ChipID].write( Offset, Data );
		break;

	case 0x03:
		if ( run_ym2151( ChipID, to_fm_time( Sample ) ) )
			ym2151[ChipID].write( Offset, Data );
		break;

	case 0x06:
		if ( run_ym2203( ChipID, to_fm_time( Sample ) ) )
			ym2203[ChipID].write( Offset, Data );
		break;

	case 0x07:
		if ( run_ym2608( ChipID, to_fm_time( Sample ) ) )
		{
			switch (Port)
			{
			case 0: ym2608[ChipID].write0( Offset, Data ); break;
			case 1: ym2608[ChipID].write1( Offset, Data ); break;
			}
		}
		break;

	case 0x08:
		if ( run_ym2610( ChipID, to_fm_time( Sample ) ) )
		{
			switch (Port)
			{
			case 0: ym2610[ChipID].write0( Offset, Data ); break;
			case 1: ym2610[ChipID].write1( Offset, Data ); break;
			}
		}
		break;

	case 0x09:
		if ( run_ym3812( ChipID, to_fm_time( Sample ) ) )
			ym3812[ChipID].write( Offset, Data );
		break;

	case 0x0C:
		if ( run_ymf262( ChipID, to_fm_time( Sample ) ) )
		{
			switch (Port)
			{
			case 0: ymf262[ChipID].write0( Offset, Data ); break;
			case 1: ymf262[ChipID].write1( Offset, Data ); break;
			}
		}
		break;

	case 0x0F:
		if ( run_ymz280b( to_fm_time( Sample ) ) )
			ymz280b.write( Offset, Data );
		break;

	case 0x12:
		ay[ChipID].write_addr( Offset );
		ay[ChipID].write_data( to_ay_time( Sample ), Data );
		break;

	case 0x13:
		gbdmg[ChipID].write_register( to_gbdmg_time( Sample ), 0xFF10 + Offset, Data );
		break;

	case 0x17:
		if ( run_okim6258( ChipID, to_fm_time( Sample ) ) )
			okim6258[ChipID].write( Offset, Data );
		break;

	case 0x18:
		if ( run_okim6295( ChipID, to_fm_time( Sample ) ) )
			okim6295[ChipID].write( Offset, Data );
		break;

	case 0x19:
		if ( run_k051649( to_fm_time( Sample ) ) )
			k051649.write( Port, Offset, Data );
		break;

	case 0x1A:
		if ( run_k054539( to_fm_time( Sample ) ) )
			k054539.write( ( Port << 8 ) | Offset, Data );
		break;

    case 0x1B:
        huc6280[ChipID].write_data( to_huc6280_time( Sample ), 0x800 + Offset, Data );
        break;

	case 0x1D:
		if ( run_k053260( to_fm_time( Sample ) ) )
			k053260.write( Offset, Data );
		break;

    case 0x1F:
        if ( run_qsound( ChipID, Sample ) )
            qsound[ ChipID ].write( Data, ( Port << 8 ) + Offset );
        break;
	}
}

void Vgm_Core::set_tempo( double t )
{
	if ( file_begin() )
	{
		vgm_rate = (int) (44100 * t + 0.5);
		blip_time_factor = (int) ((double)
				(1 << blip_time_bits) / vgm_rate * stereo_buf[0].center()->clock_rate() + 0.5);
		blip_ay_time_factor = (int) ((double)
			(1 << blip_time_bits) / vgm_rate * stereo_buf[1].center()->clock_rate() + 0.5);
        blip_huc6280_time_factor = (int) ((double)
            (1 << blip_time_bits) / vgm_rate * stereo_buf[2].center()->clock_rate() + 0.5);
		blip_gbdmg_time_factor = (int)((double)
			(1 << blip_time_bits) / vgm_rate * stereo_buf[3].center()->clock_rate() + 0.5);
		//dprintf( "blip_time_factor: %ld\n", blip_time_factor );
		//dprintf( "vgm_rate: %ld\n", vgm_rate );
		// TODO: remove? calculates vgm_rate more accurately (above differs at most by one Hz only)
		//blip_time_factor = (int) floor( double (1 << blip_time_bits) * psg_rate_ / 44100 / t + 0.5 );
		//vgm_rate = (int) floor( double (1 << blip_time_bits) * psg_rate_ / blip_time_factor + 0.5 );
		
		fm_time_factor = 2 + (int) (fm_rate * (1 << fm_time_bits) / vgm_rate + 0.5);
	}
}

bool Vgm_Core::header_t::valid_tag() const
{
	return !memcmp( tag, "Vgm ", 4 );
}

int Vgm_Core::header_t::size() const
{
	unsigned int version = get_le32( this->version );
	unsigned int data_offset;
	if ( version >= 0x150 )
	{
		data_offset = get_le32( this->data_offset );
		if ( data_offset ) data_offset += offsetof( header_t, data_offset );
	}
	else data_offset = 0x40;
	unsigned expected_size = ( version > 0x150 ) ? ( ( version > 0x160 ) ? unsigned(size_max) : unsigned(size_151) ) : unsigned(size_min);
	if ( expected_size > data_offset ) expected_size = data_offset ? (data_offset > unsigned(size_max) ? unsigned(size_max) : data_offset) : unsigned(size_min);
	return expected_size;
}

void Vgm_Core::header_t::cleanup()
{
	unsigned int version = get_le32( this->version );

	if ( size() < size_max ) memset( ((byte*)this) + size(), 0, size_max - size() );

	if ( version < 0x161 )
	{
		memset( this->gbdmg_rate, 0, size_max - offsetof(header_t, gbdmg_rate) );
	}

	if ( version < 0x160 )
	{
		volume_modifier = 0;
		reserved = 0;
		loop_base = 0;
	}

	if ( version < 0x151 ) memset( this->rf5c68_rate, 0, size_max - size_min );

	if ( version < 0x150 )
	{
		set_le32( data_offset, size_min - offsetof(header_t, data_offset) );
		sn76489_flags = 0;
		set_le32( segapcm_rate, 0 );
		set_le32( segapcm_reg, 0 );
	}

	if ( version < 0x110 )
	{
		set_le16( noise_feedback, 0 );
		noise_width = 0;
		unsigned int rate = get_le32( ym2413_rate );
		set_le32( ym2612_rate, rate );
		set_le32( ym2151_rate, rate );
	}

	if ( version < 0x101 )
	{
		set_le32( frame_rate, 0 );
	}
}

blargg_err_t Vgm_Core::load_mem_( byte const data [], int size )
{
	assert( offsetof (header_t, rf5c68_rate) == header_t::size_min );
	assert( offsetof (header_t, extra_offset[4]) == header_t::size_max );
	
	if ( size <= header_t::size_min )
		return blargg_err_file_type;

	memcpy( &_header, data, header_t::size_min );
	
	header_t const& h = header();
	
	if ( !h.valid_tag() )
		return blargg_err_file_type;

	int version = get_le32( h.version );
	
	check( version < 0x100 );

	if ( version > 0x150 )
	{
		if ( size < header().size() )
			return "Invalid header";

		memcpy( &_header.rf5c68_rate, data + offsetof (header_t, rf5c68_rate), header().size() - header_t::size_min );
	}

	_header.cleanup();

	// Get loop
	loop_begin = file_end();
	if ( get_le32( h.loop_offset ) )
		loop_begin = &data [get_le32( h.loop_offset ) + offsetof (header_t,loop_offset)];
	
	// PSG rate
	int psg_rate = get_le32( h.psg_rate ) & 0x3FFFFFFF;
	if ( !psg_rate )
		psg_rate = 3579545;
	stereo_buf[0].clock_rate( psg_rate );

	int ay_rate = get_le32( h.ay8910_rate ) & 0xBFFFFFFF;
	if ( !ay_rate )
		ay_rate = 2000000;
	stereo_buf[1].clock_rate( ay_rate * 2 );
	ay[0].set_type( (Ay_Apu::Ay_Apu_Type) header().ay8910_type );
	ay[1].set_type( (Ay_Apu::Ay_Apu_Type) header().ay8910_type );

    int huc6280_rate = get_le32( h.huc6280_rate ) & 0xBFFFFFFF;
    if ( !huc6280_rate )
        huc6280_rate = 3579545;
    stereo_buf[2].clock_rate( huc6280_rate * 2 );

	int gbdmg_rate = get_le32( h.gbdmg_rate ) & 0xBFFFFFFF;
	if ( !gbdmg_rate )
		gbdmg_rate = Gb_Apu::clock_rate;
	stereo_buf[3].clock_rate( gbdmg_rate );

	// Disable FM
	fm_rate = 0;
	ymz280b.enable( false );
	ymf262[0].enable( false );
	ymf262[1].enable( false );
	ym3812[0].enable( false );
	ym3812[1].enable( false );
	ym2612[0].enable( false );
	ym2612[1].enable( false );
	ym2610[0].enable( false );
	ym2610[1].enable( false );
	ym2608[0].enable( false );
	ym2608[1].enable( false );
	ym2413[0].enable( false );
	ym2413[1].enable( false );
	ym2203[0].enable( false );
	ym2203[1].enable( false );
	ym2151[0].enable( false );
	ym2151[1].enable( false );
	c140.enable( false );
	segapcm.enable( false );
	rf5c68.enable( false );
	rf5c164.enable( false );
	pwm.enable( false );
	okim6258[0].enable( false );
    okim6258[1].enable( false );
	okim6295[0].enable( false );
	okim6295[1].enable( false );
	k051649.enable( false );
	k053260.enable( false );
	k054539.enable( false );
    qsound[0].enable( false );
    qsound[1].enable( false );
	
	set_tempo( 1 );
	
	return blargg_ok;
}

// Update pre-1.10 header FM rates by scanning commands
void Vgm_Core::update_fm_rates( int* ym2151_rate, int* ym2413_rate, int* ym2612_rate ) const
{
	byte const* p = file_begin() + header().size();
	int data_offset = get_le32( header().data_offset );
	check( data_offset );
	if ( data_offset )
		p += data_offset + offsetof( header_t, data_offset ) - header().size();
	while ( p < file_end() )
	{
		switch ( *p )
		{
		case cmd_end:
			return;
		
		case cmd_psg:
		case cmd_byte_delay:
			p += 2;
			break;
		
		case cmd_delay:
			p += 3;
			break;
		
		case cmd_data_block:
			p += 7 + get_le32( p + 3 );
			break;

		case cmd_ram_block:
			p += 12;
			break;
		
		case cmd_ym2413:
			*ym2151_rate = 0;
			*ym2612_rate = 0;
			return;
		
		case cmd_ym2612_port0:
		case cmd_ym2612_port1:
			*ym2612_rate = *ym2413_rate;
			*ym2413_rate = 0;
			*ym2151_rate = 0;
			return;
		
		case cmd_ym2151:
			*ym2151_rate = *ym2413_rate;
			*ym2413_rate = 0;
			*ym2612_rate = 0;
			return;
		
		default:
			p += command_len( *p );
		}
	}
}

blargg_err_t Vgm_Core::init_chips( double* rate, bool reinit )
{
	int ymz280b_rate = get_le32( header().ymz280b_rate ) & 0xBFFFFFFF;
	int ymf262_rate = get_le32( header().ymf262_rate ) & 0xBFFFFFFF;
	int ym3812_rate = get_le32( header().ym3812_rate ) & 0xBFFFFFFF;
	int ym2612_rate = get_le32( header().ym2612_rate ) & 0xBFFFFFFF;
	int ym2610_rate = get_le32( header().ym2610_rate ) & 0x3FFFFFFF;
	int ym2608_rate = get_le32( header().ym2608_rate ) & 0x3FFFFFFF;
	int ym2413_rate = get_le32( header().ym2413_rate ) & 0xBFFFFFFF;
	int ym2203_rate = get_le32( header().ym2203_rate ) & 0xBFFFFFFF;
	int ym2151_rate = get_le32( header().ym2151_rate ) & 0xBFFFFFFF;
	int c140_rate = get_le32( header().c140_rate ) & 0xBFFFFFFF;
	int segapcm_rate = get_le32( header().segapcm_rate ) & 0xBFFFFFFF;
	int rf5c68_rate = get_le32( header().rf5c68_rate ) & 0xBFFFFFFF;
	int rf5c164_rate = get_le32( header().rf5c164_rate ) & 0xBFFFFFFF;
	int pwm_rate = get_le32( header().pwm_rate ) & 0xBFFFFFFF;
	int okim6258_rate = get_le32( header().okim6258_rate ) & 0xBFFFFFFF;
	int okim6295_rate = get_le32( header().okim6295_rate ) & 0xBFFFFFFF;
	int k051649_rate = get_le32( header().k051649_rate ) & 0xBFFFFFFF;
	int k053260_rate = get_le32( header().k053260_rate ) & 0xBFFFFFFF;
	int k054539_rate = get_le32( header().k054539_rate ) & 0xBFFFFFFF;
    int qsound_rate = get_le32( header().qsound_rate ) & 0xBFFFFFFF;
	if ( ym2413_rate && get_le32( header().version ) < 0x110 )
		update_fm_rates( &ym2151_rate, &ym2413_rate, &ym2612_rate );
	
	*rate = vgm_rate;

	if ( ymf262_rate )
	{
		bool dual_chip = !!(header().ymf262_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ymf262_rate / 288.0;
		int result;
		if ( !reinit )
		{
			result = ymf262[0].set_rate( fm_rate, ymf262_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ymf262[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ymf262[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ymf262[1].set_rate( fm_rate, ymf262_rate );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ymf262[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ymf262[1].enable();
		}
	}
	if ( ym3812_rate )
	{
		bool dual_chip = !!(header().ym3812_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym3812_rate / 72.0;
		int result;
		if ( !reinit )
		{
			result = ym3812[0].set_rate( fm_rate, ym3812_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ym3812[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym3812[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym3812[1].set_rate( fm_rate, ym3812_rate );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ym3812[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym3812[1].enable();
		}
	}
	if ( ym2612_rate )
	{
		bool dual_chip = !!(header().ym2612_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym2612_rate / 144.0;
		if ( !reinit )
		{
			RETURN_ERR( ym2612[0].set_rate( fm_rate, ym2612_rate ) );
		}
		RETURN_ERR( ym2612[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2612[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				RETURN_ERR( ym2612[1].set_rate( fm_rate, ym2612_rate ) );
			}
			RETURN_ERR( ym2612[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2612[1].enable();
		}
	}
	if ( ym2610_rate )
	{
		bool dual_chip = !!(header().ym2610_rate[3] & 0x40);
		bool is_2610b = !!(header().ym2610_rate[3] & 0x80);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym2610_rate / 72.0;
		int result;
		if ( !reinit )
		{
			result = ym2610[0].set_rate( fm_rate, ym2610_rate, is_2610b );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ym2610[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2610[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym2610[1].set_rate( fm_rate, ym2610_rate, is_2610b );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ym2610[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2610[1].enable();
		}
	}
	if ( ym2608_rate )
	{
		bool dual_chip = !!(header().ym2610_rate[3] & 0x40);
		double gain = dual_chip ? 1.0 : 2.0;
		double fm_rate = ym2608_rate / 72.0;
		int result;
		if ( !reinit )
		{
			result = ym2608[0].set_rate( fm_rate, ym2608_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ym2608[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2608[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym2608[1].set_rate( fm_rate, ym2608_rate );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ym2608[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2608[1].enable();
		}
	}
	if ( ym2413_rate )
	{
		bool dual_chip = !!(header().ym2413_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym2413_rate / 72.0;
		int result;
		if ( !reinit )
		{
			result = ym2413[0].set_rate( fm_rate, ym2413_rate );
			if ( result == 2 )
				return "YM2413 FM sound not supported";
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ym2413[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2413[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym2413[1].set_rate( fm_rate, ym2413_rate );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ym2413[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2413[1].enable();
		}
	}
	if ( ym2151_rate )
	{
		bool dual_chip = !!(header().ym2151_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym2151_rate / 64.0;
		int result;
		if ( !reinit )
		{
			result = ym2151[0].set_rate( fm_rate, ym2151_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( ym2151[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2151[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym2151[1].set_rate( fm_rate, ym2151_rate );
				CHECK_ALLOC( !result );
			}
			RETURN_ERR( ym2151[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2151[1].enable();
		}
	}
	if ( ym2203_rate )
	{
		bool dual_chip = !!(header().ym2203_rate[3] & 0x40);
		double gain = dual_chip ? 0.5 : 1.0;
		double fm_rate = ym2203_rate / 72.0;
		int result;
		if ( !reinit )
		{
			result = ym2203[0].set_rate( fm_rate, ym2203_rate );
			CHECK_ALLOC ( !result );
		}
		RETURN_ERR( ym2203[0].setup( fm_rate / vgm_rate, 0.85, gain ) );
		ym2203[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				result = ym2203[1].set_rate( fm_rate, ym2203_rate );
				CHECK_ALLOC ( !result );
			}
			RETURN_ERR( ym2203[1].setup( fm_rate / vgm_rate, 0.85, gain ) );
			ym2203[1].enable();
		}
	}

	if ( segapcm_rate )
	{
		double pcm_rate = segapcm_rate / 128.0;
		if ( !reinit )
		{
			int result = segapcm.set_rate( get_le32( header().segapcm_reg ) );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( segapcm.setup( pcm_rate / vgm_rate, 0.85, 1.5 ) );
		segapcm.enable();
	}
	if ( rf5c68_rate )
	{
		double pcm_rate = rf5c68_rate / 384.0;
		if ( !reinit )
		{
			int result = rf5c68.set_rate();
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( rf5c68.setup( pcm_rate / vgm_rate, 0.85, 0.6875 ) );
		rf5c68.enable();
	}
	if ( rf5c164_rate )
	{
		double pcm_rate = rf5c164_rate / 384.0;
		if ( !reinit )
		{
			int result = rf5c164.set_rate( rf5c164_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( rf5c164.setup( pcm_rate / vgm_rate, 0.85, 0.5 ) );
		rf5c164.enable();
	}
	if ( pwm_rate )
	{
		double pcm_rate = 22020.0;
		if ( !reinit )
		{
			int result = pwm.set_rate( pwm_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( pwm.setup( pcm_rate / vgm_rate, 0.85, 0.875 ) );
		pwm.enable();
	}
	if ( okim6258_rate )
	{
        bool dual_chip = !!( header().okim6258_rate[3] & 0x40 );
		if ( !reinit )
		{
			okim6258_hz[0] = okim6258[0].set_rate( okim6258_rate, header().okim6258_flags & 0x03, ( header().okim6258_flags & 0x04 ) >> 2, ( header().okim6258_flags & 0x08 ) >> 3 );
			CHECK_ALLOC( okim6258_hz[0] );
		}
		RETURN_ERR( okim6258[0].setup( (double)okim6258_hz[0] / vgm_rate, 0.85, 1.0 ) );
		okim6258[0].enable();
        if ( dual_chip )
        {
            if ( !reinit )
            {
                okim6258_hz[1] = okim6258[1].set_rate( okim6258_rate, header().okim6258_flags & 0x03, ( header().okim6258_flags & 0x04 ) >> 2, ( header().okim6258_flags & 0x08 ) >> 3 );
                CHECK_ALLOC( okim6258_hz[1] );
            }
            RETURN_ERR( okim6258[1].setup( (double)okim6258_hz[1] / vgm_rate, 0.85, 1.0 ) );
            okim6258[1].enable();
        }
	}
	if ( okim6295_rate )
	{
		// moo
		Mem_File_Reader rdr( file_begin(), file_size() );
		Music_Emu * vgm = gme_vgm_type->new_info();
		track_info_t info;
		vgm->load( rdr );
		vgm->track_info( &info, 0 );
		delete vgm;

		bool is_cp_system = strncmp( info.system, "CP", 2 ) == 0;
		bool dual_chip = !!( header().okim6295_rate[3] & 0x40 );
		double gain = is_cp_system ? 0.4296875 : 1.0;
		if ( dual_chip ) gain *= 0.5;
		if ( !reinit )
		{
			okim6295_hz = okim6295[0].set_rate( okim6295_rate );
			CHECK_ALLOC( okim6295_hz );
		}
		RETURN_ERR( okim6295[0].setup( (double)okim6295_hz / vgm_rate, 0.85, gain ) );
		okim6295[0].enable();
		if ( dual_chip )
		{
			if ( !reinit )
			{
				int result = okim6295[1].set_rate( okim6295_rate );
				CHECK_ALLOC( result );
			}
			RETURN_ERR( okim6295[1].setup( (double)okim6295_hz / vgm_rate, 0.85, gain ) );
			okim6295[1].enable();
		}
	}
	if ( c140_rate )
	{
		double pcm_rate = c140_rate;
		if ( !reinit )
		{
			int result = c140.set_rate( header().c140_type, c140_rate, c140_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( c140.setup( pcm_rate / vgm_rate, 0.85, 1.0 ) );
		c140.enable();
	}
	if ( k051649_rate )
	{
		double pcm_rate = k051649_rate / 16.0;
		if ( !reinit )
		{
			int result = k051649.set_rate( k051649_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( k051649.setup( pcm_rate / vgm_rate, 0.85, 1.0 ) );
		k051649.enable();
	}
	if ( k053260_rate )
	{
		double pcm_rate = k053260_rate / 32.0;
		if ( !reinit )
		{
			int result = k053260.set_rate( k053260_rate );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( k053260.setup( pcm_rate / vgm_rate, 0.85, 1.0 ) );
		k053260.enable();
	}
	if ( k054539_rate )
	{
		double pcm_rate = k054539_rate;
		if ( !reinit )
		{
			int result = k054539.set_rate( k054539_rate, header().k054539_flags );
			CHECK_ALLOC( !result );
		}
		RETURN_ERR( k054539.setup( pcm_rate / vgm_rate, 0.85, 1.0 ) );
		k054539.enable();
	}
	if ( ymz280b_rate )
	{
		if ( !reinit )
		{
			ymz280b_hz = ymz280b.set_rate( ymz280b_rate );
			CHECK_ALLOC( ymz280b_hz );
		}
		RETURN_ERR( ymz280b.setup( (double)ymz280b_hz / vgm_rate, 0.85, 0.59375 ) );
		ymz280b.enable();
	}
    if ( qsound_rate )
    {
        /*double pcm_rate = (double)qsound_rate / 166.0;*/
		if ( !reinit )
		{
			int result = qsound[0].set_rate( qsound_rate );
			CHECK_ALLOC( result );
		}
		qsound[0].set_sample_rate( vgm_rate );
        RETURN_ERR( qsound[0].setup( 1.0, 0.85, 1.0 ) );
        qsound[0].enable();
    }

	fm_rate = *rate;
	
	return blargg_ok;
}

void Vgm_Core::start_track()
{
	psg[0].reset( get_le16( header().noise_feedback ), header().noise_width );
	psg[1].reset( get_le16( header().noise_feedback ), header().noise_width );
	ay[0].reset();
	ay[1].reset();
    huc6280[0].reset();
    huc6280[1].reset();
	gbdmg[0].reset();
	gbdmg[1].reset();
	
	blip_buf[0] = stereo_buf[0].center();
	blip_buf[1] = blip_buf[0];

	dac_disabled[0] = -1;
	dac_disabled[1] = -1;
	pos             = file_begin() + header().size();
	dac_amp[0]      = -1;
	dac_amp[1]      = -1;
	vgm_time        = 0;
	int data_offset = get_le32( header().data_offset );
	check( data_offset );
	if ( data_offset )
		pos += data_offset + offsetof (header_t,data_offset) - header().size();
	pcm_pos      = pos;
	
	if ( uses_fm() )
	{
		if ( rf5c68.enabled() )
			rf5c68.reset();

		if ( rf5c164.enabled() )
			rf5c164.reset();

		if ( segapcm.enabled() )
			segapcm.reset();

		if ( pwm.enabled() )
			pwm.reset();

		if ( okim6258[0].enabled() )
			okim6258[0].reset();
        
        if ( okim6258[1].enabled() )
            okim6258[1].reset();

		if ( okim6295[0].enabled() )
			okim6295[0].reset();

		if ( okim6295[1].enabled() )
			okim6295[1].reset();

		if ( k051649.enabled() )
			k051649.reset();

		if ( k053260.enabled() )
			k053260.reset();

		if ( k054539.enabled() )
			k054539.reset();

		if ( c140.enabled() )
			c140.reset();

		if ( ym2151[0].enabled() )
			ym2151[0].reset();

		if ( ym2151[1].enabled() )
			ym2151[1].reset();

		if ( ym2203[0].enabled() )
			ym2203[0].reset();

		if ( ym2203[1].enabled() )
			ym2203[1].reset();

		if ( ym2413[0].enabled() )
			ym2413[0].reset();

		if ( ym2413[1].enabled() )
			ym2413[1].reset();
		
		if ( ym2612[0].enabled() )
			ym2612[0].reset();

		if ( ym2612[1].enabled() )
			ym2612[1].reset();

		if ( ym2610[0].enabled() )
			ym2610[0].reset();

		if ( ym2610[1].enabled() )
			ym2610[1].reset();

		if ( ym2608[0].enabled() )
			ym2608[0].reset();

		if ( ym2608[1].enabled() )
			ym2608[1].reset();

		if ( ym3812[0].enabled() )
			ym3812[0].reset();

		if ( ym3812[1].enabled() )
			ym3812[1].reset();

		if ( ymf262[0].enabled() )
			ymf262[0].reset();

		if ( ymf262[1].enabled() )
			ymf262[1].reset();

		if ( ymz280b.enabled() )
			ymz280b.reset();

        if ( qsound[0].enabled() )
            qsound[0].reset();

        if ( qsound[1].enabled() )
            qsound[1].reset();
		
		stereo_buf[0].clear();
		stereo_buf[1].clear();
        stereo_buf[2].clear();
		stereo_buf[3].clear();
	}

	for ( unsigned i = 0; i < DacCtrlUsed; i++ )
	{
		device_reset_daccontrol( dac_control [i] );
		DacCtrlTime[DacCtrlMap[i]] = 0;
	}
	
	for ( unsigned i = 0; i < PCM_BANK_COUNT; i++)
	{
		// reset PCM Bank, but not the data
		// (this way I don't need to decompress the data again when restarting)
		PCMBank [i].DataPos = 0;
		PCMBank [i].BnkPos = 0;
	}
	PCMTbl.EntryCount = 0;

	fm_time_offset = 0;
	ay_time_offset = 0;
    huc6280_time_offset = 0;
	gbdmg_time_offset = 0;

    dac_control_recursion = 0;
}

inline Vgm_Core::fm_time_t Vgm_Core::to_fm_time( vgm_time_t t ) const
{
	return (t * fm_time_factor + fm_time_offset) >> fm_time_bits;
}

inline blip_time_t Vgm_Core::to_psg_time( vgm_time_t t ) const
{
	return (t * blip_time_factor) >> blip_time_bits;
}

inline blip_time_t Vgm_Core::to_ay_time( vgm_time_t t ) const
{
    return (t * blip_ay_time_factor) >> blip_time_bits;
}

inline blip_time_t Vgm_Core::to_huc6280_time( vgm_time_t t ) const
{
    return (t * blip_huc6280_time_factor) >> blip_time_bits;
}

inline blip_time_t Vgm_Core::to_gbdmg_time( vgm_time_t t ) const
{
	return (t * blip_gbdmg_time_factor) >> blip_time_bits;
}

void Vgm_Core::write_pcm( vgm_time_t vgm_time, int chip, int amp )
{
	chip = !!chip;
	if ( blip_buf[chip] )
	{
		check( amp >= 0 );
		blip_time_t blip_time = to_psg_time( vgm_time );
		int old = dac_amp[chip];
		int delta = amp - old;
		dac_amp[chip] = amp;
		blip_buf[chip]->set_modified();
		if ( old >= 0 ) // first write is ignored, to avoid click
			pcm.offset_inline( blip_time, delta, blip_buf[chip] );
		else
			dac_amp[chip] |= dac_disabled[chip];
	}
}

blip_time_t Vgm_Core::run( vgm_time_t end_time )
{
	vgm_time_t vgm_time = this->vgm_time; 
	vgm_time_t vgm_loop_time = ~0;
	int ChipID;
	byte const* pos = this->pos;
	if ( pos > file_end() )
		set_warning( "Stream lacked end event" );
	
	while ( vgm_time < end_time && pos < file_end() )
	{
		// TODO: be sure there are enough bytes left in stream for particular command
		// so we don't read past end
		switch ( *pos++ )
		{
		case cmd_end:
			if ( vgm_loop_time == ~0 ) vgm_loop_time = vgm_time;
			else if ( vgm_loop_time == vgm_time ) loop_begin = file_end(); // XXX some files may loop forever on a region without any delay commands
			pos = loop_begin; // if not looped, loop_begin == file_end()
			if ( pos != file_end() ) has_looped = true;
			break;
		
		case cmd_delay_735:
			vgm_time += 735;
			break;
		
		case cmd_delay_882:
			vgm_time += 882;
			break;
		
		case cmd_gg_stereo:
			psg[0].write_ggstereo( to_psg_time( vgm_time ), *pos++ );
			break;

		case cmd_gg_stereo_2:
			psg[1].write_ggstereo( to_psg_time( vgm_time ), *pos++ );
			break;
		
		case cmd_psg:
			psg[0].write_data( to_psg_time( vgm_time ), *pos++ );
			break;

		case cmd_psg_2:
			psg[1].write_data( to_psg_time( vgm_time ), *pos++ );
			break;

		case cmd_ay8910:
			ChipID = !!(pos [0] & 0x80);
			chip_reg_write( vgm_time, 0x12, ChipID, 0x00, pos [0] & 0x7F, pos [1] );
			pos += 2;
			break;
		
		case cmd_delay:
			vgm_time += pos [1] * 0x100 + pos [0];
			pos += 2;
			break;
		
		case cmd_byte_delay:
			vgm_time += *pos++;
			break;

		case cmd_segapcm_write:
			if ( get_le32( header().segapcm_rate ) > 0 )
				if ( run_segapcm( to_fm_time( vgm_time ) ) )
					segapcm.write( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_rf5c68:
			if ( run_rf5c68( to_fm_time( vgm_time ) ) )
				rf5c68.write( pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_rf5c68_mem:
			if ( run_rf5c68( to_fm_time( vgm_time ) ) )
				rf5c68.write_mem( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_rf5c164:
			if ( run_rf5c164( to_fm_time( vgm_time ) ) )
				rf5c164.write( pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_rf5c164_mem:
			if ( run_rf5c164( to_fm_time( vgm_time ) ) )
				rf5c164.write_mem( get_le16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_pwm:
			chip_reg_write( vgm_time, 0x11, 0x00, pos [0] >> 4, pos [0] & 0x0F, pos [1] );
			pos += 2;
			break;

		case cmd_c140:
			if ( get_le32( header().c140_rate ) > 0 )
				if ( run_c140( to_fm_time( vgm_time ) ) )
					c140.write( get_be16( pos ), pos [2] );
			pos += 3;
			break;

		case cmd_ym2151:
			chip_reg_write( vgm_time, 0x03, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2151_2:
			chip_reg_write( vgm_time, 0x03, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2203:
			chip_reg_write( vgm_time, 0x06, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2203_2:
			chip_reg_write( vgm_time, 0x06, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2413:
			chip_reg_write( vgm_time, 0x01, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2413_2:
			chip_reg_write( vgm_time, 0x01, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym3812:
			chip_reg_write( vgm_time, 0x09, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym3812_2:
			chip_reg_write( vgm_time, 0x09, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ymf262_port0:
			chip_reg_write( vgm_time, 0x0C, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ymf262_2_port0:
			chip_reg_write( vgm_time, 0x0C, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ymf262_port1:
			chip_reg_write( vgm_time, 0x0C, 0x00, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ymf262_2_port1:
			chip_reg_write( vgm_time, 0x0C, 0x01, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ymz280b:
			chip_reg_write( vgm_time, 0x0F, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2612_port0:
			chip_reg_write( vgm_time, 0x02, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2612_2_port0:
			chip_reg_write( vgm_time, 0x02, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;
		
		case cmd_ym2612_port1:
			chip_reg_write( vgm_time, 0x02, 0x00, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2612_2_port1:
			chip_reg_write( vgm_time, 0x02, 0x01, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2610_port0:
			chip_reg_write( vgm_time, 0x08, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2610_2_port0:
			chip_reg_write( vgm_time, 0x08, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2610_port1:
			chip_reg_write( vgm_time, 0x08, 0x00, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2610_2_port1:
			chip_reg_write( vgm_time, 0x08, 0x01, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2608_port0:
			chip_reg_write( vgm_time, 0x07, 0x00, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2608_2_port0:
			chip_reg_write( vgm_time, 0x07, 0x01, 0x00, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2608_port1:
			chip_reg_write( vgm_time, 0x07, 0x00, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_ym2608_2_port1:
			chip_reg_write( vgm_time, 0x07, 0x01, 0x01, pos [0], pos [1] );
			pos += 2;
			break;

		case cmd_okim6258_write:
			chip_reg_write( vgm_time, 0x17, ChipID, 0x00, pos [0] & 0x7F, pos [1] );
			pos += 2;
			break;

		case cmd_okim6295_write:
			ChipID = (pos [0] & 0x80) ? 1 : 0;
			chip_reg_write( vgm_time, 0x18, ChipID, 0x00, pos [0] & 0x7F, pos [1] );
			pos += 2;
			break;

        case cmd_huc6280_write:
            ChipID = (pos [0] & 0x80) ? 1 : 0;
            chip_reg_write( vgm_time, 0x1B, ChipID, 0x00, pos [0] & 0x7F, pos [1] );
            pos += 2;
            break;

		case cmd_gbdmg_write:
			ChipID = (pos [0] & 0x80) ? 1 : 0;
			chip_reg_write( vgm_time, 0x13, ChipID, 0x00, pos [0] & 0x7F, pos [1] );
			pos += 2;
			break;

		case cmd_k051649_write:
			chip_reg_write( vgm_time, 0x19, 0x00, pos [0] & 0x7F, pos [1], pos [2] );
			pos += 3;
			break;

		case cmd_k053260_write:
			chip_reg_write( vgm_time, 0x1D, 0x00, 0x00, pos [0] & 0x7F, pos [1] );
			pos += 2;
			break;

		case cmd_k054539_write:
			chip_reg_write( vgm_time, 0x1A, 0x00, pos [0] & 0x7F, pos [1], pos [2] );
			pos += 3;
			break;

        case cmd_qsound_write:
            chip_reg_write( vgm_time, 0x1F, 0x00, pos [0], pos [1], pos [2] );
            pos += 3;
            break;
			
		case cmd_dacctl_setup:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF )
				{
					if ( ! DacCtrl [chip].Enable )
					{
						dac_control_grow( chip );
						DacCtrl [chip].Enable = true;
					}
					daccontrol_setup_chip( dac_control [DacCtrlMap [chip]], pos [1] & 0x7F, ( pos [1] & 0x80 ) >> 7, get_be16( pos + 2 ) );
				}
			}
			pos += 4;
			break;

		case cmd_dacctl_data:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF && DacCtrl [chip].Enable )
				{
					DacCtrl [chip].Bank = pos [1];
					if ( DacCtrl [chip].Bank >= 0x40 )
						DacCtrl [chip].Bank = 0x00;

					VGM_PCM_BANK * TempPCM = &PCMBank [DacCtrl [chip].Bank];
					daccontrol_set_data( dac_control [DacCtrlMap [chip]], TempPCM->Data, TempPCM->DataSize, pos [2], pos [3] );
				}
			}
			pos += 4;
			break;
		case cmd_dacctl_freq:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF && DacCtrl [chip].Enable )
				{
					daccontrol_set_frequency( dac_control [DacCtrlMap [chip]], get_le32( pos + 1 ) );
				}
			}
			pos += 5;
			break;
		case cmd_dacctl_play:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF && DacCtrl [chip].Enable && PCMBank [DacCtrl [chip].Bank].BankCount )
				{
					daccontrol_start( dac_control [DacCtrlMap [chip]], get_le32( pos + 1 ), pos [5], get_le32( pos + 6 ) );
				}
			}
			pos += 10;
			break;
		case cmd_dacctl_stop:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF && DacCtrl [chip].Enable )
				{
					daccontrol_stop( dac_control [DacCtrlMap [chip]] );
				}
				else if ( chip == 0xFF )
				{
					for ( unsigned i = 0; i < DacCtrlUsed; i++ )
					{
						daccontrol_stop( dac_control [i] );
					}
				}
			}
			pos++;
			break;
		case cmd_dacctl_playblock:
			if ( run_dac_control( vgm_time ) )
			{
				unsigned chip = pos [0];
				if ( chip < 0xFF && DacCtrl [chip].Enable && PCMBank [DacCtrl [chip].Bank].BankCount )
				{
					VGM_PCM_BANK * TempPCM = &PCMBank [DacCtrl [chip].Bank];
					unsigned block_number = get_le16( pos + 1 );
					if ( block_number >= TempPCM->BankCount )
						block_number = 0;
					VGM_PCM_DATA * TempBnk = &TempPCM->Bank [block_number];
					unsigned flags = DCTRL_LMODE_BYTES | ((pos [4] & 1) << 7);
					daccontrol_start( dac_control [DacCtrlMap [chip]], TempBnk->DataStart, flags, TempBnk->DataSize );
				}
			}
			pos += 4;
			break;

		case cmd_data_block: {
			check( *pos == cmd_end );
			int type = pos [1];
			int size = get_le32( pos + 2 );
			int chipid = 0;
			if ( size & 0x80000000 )
			{
				size &= 0x7FFFFFFF;
				chipid = 1;
			}
			pos += 6;
			switch ( type & 0xC0 )
			{
			case pcm_block_type:
			case pcm_aux_block_type:
				AddPCMData( type, size, pos );
				break;

			case rom_block_type:
				if ( size >= 8 )
				{
					int rom_size = get_le32( pos );
					int data_start = get_le32( pos + 4 );
					int data_size = size - 8;
					void * rom_data = ( void * ) ( pos + 8 );

					switch ( type )
					{
					case rom_segapcm:
						if ( segapcm.enabled() )
							segapcm.write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_ym2608_deltat:
						if ( ym2608[chipid].enabled() )
						{
							ym2608[chipid].write_rom( 0x02, rom_size, data_start, data_size, rom_data );
						}
						break;

					case rom_ym2610_adpcm:
					case rom_ym2610_deltat:
						if ( ym2610[chipid].enabled() )
						{
							int rom_id = 0x01 + ( type - rom_ym2610_adpcm );
							ym2610[chipid].write_rom( rom_id, rom_size, data_start, data_size, rom_data );
						}
						break;

					case rom_ymz280b:
						if ( ymz280b.enabled() )
							ymz280b.write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_okim6295:
						if ( okim6295[chipid].enabled() )
							okim6295[chipid].write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_k054539:
						if ( k054539.enabled() )
							k054539.write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_c140:
						if ( c140.enabled() )
							c140.write_rom( rom_size, data_start, data_size, rom_data );
						break;

					case rom_k053260:
						if ( k053260.enabled() )
							k053260.write_rom( rom_size, data_start, data_size, rom_data );
						break;

                    case rom_qsound:
                        if ( qsound[chipid].enabled() )
                            qsound[chipid].write_rom( rom_size, data_start, data_size, rom_data );
                        break;
					}
				}
				break;

			case ram_block_type:
				if ( size >= 2 )
				{
					int data_start = get_le16( pos );
					int data_size = size - 2;
					void * ram_data = ( void * ) ( pos + 2 );

					switch ( type )
					{
					case ram_rf5c68:
						if ( rf5c68.enabled() )
							rf5c68.write_ram( data_start, data_size, ram_data );
						break;

					case ram_rf5c164:
						if ( rf5c164.enabled() )
							rf5c164.write_ram( data_start, data_size, ram_data );
						break;
					}
				}
				break;
			}
			pos += size;
			break;
		}

		case cmd_ram_block: {
			check( *pos == cmd_end );
			int type = pos[ 1 ];
			int data_start = get_le24( pos + 2 );
			int data_addr = get_le24( pos + 5 );
			int data_size = get_le24( pos + 8 );
			if ( !data_size ) data_size += 0x01000000;
			void * data_ptr = (void *) GetPointerFromPCMBank( type, data_start );
			switch ( type )
			{
			case rf5c68_ram_block:
				if ( rf5c68.enabled() )
					rf5c68.write_ram( data_addr, data_size, data_ptr );
				break;

			case rf5c164_ram_block:
				if ( rf5c164.enabled() )
					rf5c164.write_ram( data_addr, data_size, data_ptr );
				break;
			}
			pos += 11;
			break;
		}
		
		case cmd_pcm_seek:
			pcm_pos = GetPointerFromPCMBank( 0, get_le32( pos ) );
			pos += 4;
			break;
		
		default:
			int cmd = pos [-1];
			switch ( cmd & 0xF0 )
			{
				case cmd_pcm_delay:
					chip_reg_write( vgm_time, 0x02, 0x00, 0x00, ym2612_dac_port, *pcm_pos++ );
					vgm_time += cmd & 0x0F;
					break;
				
				case cmd_short_delay:
					vgm_time += (cmd & 0x0F) + 1;
					break;
				
				case 0x50:
					pos += 2;
					break;
				
				default:
					pos += command_len( cmd ) - 1;
					set_warning( "Unknown stream event" );
			}
		}
	}
	vgm_time -= end_time;
	this->pos = pos;
	this->vgm_time = vgm_time;
	
	return to_psg_time( end_time );
}

blip_time_t Vgm_Core::run_psg( int msec )
{
	blip_time_t t = run( msec * vgm_rate / 1000 );
	psg[0].end_frame( t );
	psg[1].end_frame( t );
	return t;
}

int Vgm_Core::play_frame( blip_time_t blip_time, int sample_count, blip_sample_t out [] )
{
	// to do: timing is working mostly by luck
	int min_pairs = (unsigned) sample_count / 2;
	int vgm_time = (min_pairs << fm_time_bits) / fm_time_factor - 1;
	assert( to_fm_time( vgm_time ) <= min_pairs );
	int pairs;
	while ( (pairs = to_fm_time( vgm_time )) < min_pairs )
		vgm_time++;
	//dprintf( "pairs: %d, min_pairs: %d\n", pairs, min_pairs );
	
    memset( out, 0, pairs * stereo * sizeof *out );

	if ( ymf262[0].enabled() )
	{
		ymf262[0].begin_frame( out );
		if ( ymf262[1].enabled() )
		{
			ymf262[1].begin_frame( out );
		}
	}
	if ( ym3812[0].enabled() )
	{
		ym3812[0].begin_frame( out );
		if ( ym3812[1].enabled() )
		{
			ym3812[1].begin_frame( out );
		}
	}
	if ( ym2612[0].enabled() )
	{
		ym2612[0].begin_frame( out );
		if ( ym2612[1].enabled() )
		{
			ym2612[1].begin_frame( out );
		}
	}
	if ( ym2610[0].enabled() )
	{
		ym2610[0].begin_frame( out );
		if ( ym2610[1].enabled() )
		{
			ym2610[1].begin_frame( out );
		}
	}
	if ( ym2608[0].enabled() )
	{
		ym2608[0].begin_frame( out );
		if ( ym2608[1].enabled() )
		{
			ym2608[1].begin_frame( out );
		}
	}
	if ( ym2413[0].enabled() )
	{
		ym2413[0].begin_frame( out );
		if ( ym2413[1].enabled() )
		{
			ym2413[1].begin_frame( out );
		}
	}
	if ( ym2203[0].enabled() )
	{
		ym2203[0].begin_frame( out );
		if ( ym2203[1].enabled() )
		{
			ym2203[1].begin_frame( out );
		}
	}
	if ( ym2151[0].enabled() )
	{
		ym2151[0].begin_frame( out );
		if ( ym2151[1].enabled() )
		{
			ym2151[1].begin_frame( out );
		}
	}

	if ( c140.enabled() )
	{
		c140.begin_frame( out );
	}
	if ( segapcm.enabled() )
	{
		segapcm.begin_frame( out );
	}
	if ( rf5c68.enabled() )
	{
		rf5c68.begin_frame( out );
	}
	if ( rf5c164.enabled() )
	{
		rf5c164.begin_frame( out );
	}
	if ( pwm.enabled() )
	{
		pwm.begin_frame( out );
	}
	if ( okim6258[0].enabled() )
	{
		okim6258[0].begin_frame( out );
        if ( okim6258[1].enabled() )
        {
            okim6258[1].begin_frame( out );
        }
	}
	if ( okim6295[0].enabled() )
	{
		okim6295[0].begin_frame( out );
		if ( okim6295[1].enabled() )
		{
			okim6295[1].begin_frame( out );
		}
	}
	if ( k051649.enabled() )
	{
		k051649.begin_frame( out );
	}
	if ( k053260.enabled() )
	{
		k053260.begin_frame( out );
	}
	if ( k054539.enabled() )
	{
		k054539.begin_frame( out );
	}
	if ( ymz280b.enabled() )
	{
		ymz280b.begin_frame( out );
	}
    if ( qsound[0].enabled() )
    {
        qsound[0].begin_frame( out );
        if ( qsound[1].enabled() )
        {
            qsound[1].begin_frame( out );
        }
    }

	run( vgm_time );

	run_dac_control( vgm_time );

	run_ymf262( 0, pairs ); run_ymf262( 1, pairs );
	run_ym3812( 0, pairs ); run_ym3812( 1, pairs );
	run_ym2612( 0, pairs ); run_ym2612( 1, pairs );
	run_ym2610( 0, pairs ); run_ym2610( 1, pairs );
	run_ym2608( 0, pairs ); run_ym2608( 1, pairs );
	run_ym2413( 0, pairs ); run_ym2413( 1, pairs );
	run_ym2203( 0, pairs ); run_ym2203( 1, pairs );
	run_ym2151( 0, pairs ); run_ym2151( 1, pairs );
	run_c140( pairs );
	run_segapcm( pairs );
	run_rf5c68( pairs );
	run_rf5c164( pairs );
	run_pwm( pairs );
	run_okim6258( 0, pairs ); run_okim6258( 1, pairs );
	run_okim6295( 0, pairs ); run_okim6295( 1, pairs );
	run_k051649( pairs );
	run_k053260( pairs );
	run_k054539( pairs );
	run_ymz280b( pairs );
    run_qsound( 0, pairs ); run_qsound( 1, pairs );
	
	fm_time_offset = (vgm_time * fm_time_factor + fm_time_offset) - (pairs << fm_time_bits);
	
	psg[0].end_frame( blip_time );
	psg[1].end_frame( blip_time );

	ay_time_offset = (vgm_time * blip_ay_time_factor + ay_time_offset) - (pairs << blip_time_bits);

	blip_time_t ay_end_time = to_ay_time( vgm_time );
	ay[0].end_frame( ay_end_time );
	ay[1].end_frame( ay_end_time );

    huc6280_time_offset = (vgm_time * blip_huc6280_time_factor + huc6280_time_offset) - (pairs << blip_time_bits);

    blip_time_t huc6280_end_time = to_huc6280_time( vgm_time );
    huc6280[0].end_frame( huc6280_end_time );
    huc6280[1].end_frame( huc6280_end_time );

	gbdmg_time_offset = (vgm_time * blip_gbdmg_time_factor + gbdmg_time_offset) - (pairs << blip_time_bits);

	blip_time_t gbdmg_end_time = to_gbdmg_time( vgm_time );
	gbdmg[0].end_frame( gbdmg_end_time );
	gbdmg[1].end_frame( gbdmg_end_time );

	memset( DacCtrlTime, 0, sizeof(DacCtrlTime) );
	
	return pairs * stereo;
}
