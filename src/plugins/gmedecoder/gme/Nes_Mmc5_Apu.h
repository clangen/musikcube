// NES MMC5 sound chip emulator

// Nes_Snd_Emu $vers
#ifndef NES_MMC5_APU_H
#define NES_MMC5_APU_H

#include "blargg_common.h"
#include "Nes_Apu.h"

class Nes_Mmc5_Apu : public Nes_Apu {
public:
	enum { regs_addr = 0x5000 };
	enum { regs_size = 0x16 };
	
	enum { osc_count  = 3 };
	void write_register( blip_time_t, unsigned addr, int data );
	void set_output( Blip_Buffer* );
	void set_output( int index, Blip_Buffer* );
	
	enum { exram_size = 1024 };
	unsigned char exram [exram_size];
	
	BLARGG_DEPRECATED_TEXT( enum { start_addr = 0x5000 }; )
	BLARGG_DEPRECATED_TEXT( enum { end_addr   = 0x5015 }; )
};

inline void Nes_Mmc5_Apu::set_output( int i, Blip_Buffer* b )
{
	// in: square 1, square 2, PCM
	// out: square 1, square 2, skipped, skipped, PCM
	if ( i > 1 )
		i += 2;
	Nes_Apu::set_output( i, b );
}

inline void Nes_Mmc5_Apu::set_output( Blip_Buffer* b )
{
	set_output( 0, b );
	set_output( 1, b );
	set_output( 2, b );
}

inline void Nes_Mmc5_Apu::write_register( blip_time_t time, unsigned addr, int data )
{
	switch ( addr )
	{
	case 0x5015: // channel enables
		data &= 0x03; // enable the square waves only
		// fall through
	case 0x5000: // Square 1
	case 0x5002:
	case 0x5003:
	case 0x5004: // Square 2
	case 0x5006:
	case 0x5007:
	case 0x5011: // DAC
		Nes_Apu::write_register( time, addr - 0x1000, data );
		break;
	
	case 0x5010: // some things write to this for some reason
		break;
	
#ifdef BLARGG_DEBUG_H
	default:
			dprintf( "Unmapped MMC5 APU write: $%04X <- $%02X\n", addr, data );
#endif
	}
}

#endif
