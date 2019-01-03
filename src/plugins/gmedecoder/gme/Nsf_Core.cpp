// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Nsf_Core.h"

#include "blargg_endian.h"

#if !NSF_EMU_APU_ONLY
	#include "Nes_Namco_Apu.h"
	#include "Nes_Vrc6_Apu.h"
	#include "Nes_Fme7_Apu.h"
	#include "Nes_Fds_Apu.h"
	#include "Nes_Mmc5_Apu.h"
	#include "Nes_Vrc7_Apu.h"
#endif

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

Nsf_Core::Nsf_Core()
{
	fds   = NULL;
	fme7  = NULL;
	mmc5  = NULL;
	namco = NULL;
	vrc6  = NULL;
	vrc7  = NULL;
}

Nsf_Core::~Nsf_Core()
{
	unload();
}

void Nsf_Core::unload()
{
#if !NSF_EMU_APU_ONLY
	delete fds;
	fds = NULL;
	
	delete fme7;
	fme7 = NULL;
	
	delete namco;
	namco = NULL;
	
	delete mmc5;
	mmc5 = NULL;
	
	delete vrc6;
	vrc6 = NULL;
	
	delete vrc7;
	vrc7 = NULL;
#endif

	Nsf_Impl::unload();
}

void Nsf_Core::set_tempo( double t )
{
	set_play_period( (int) (header().play_period() / t) );
	nes_apu()->set_tempo( t );
#if !NSF_EMU_APU_ONLY
	if ( fds )
		fds->set_tempo( t );
#endif
}

blargg_err_t Nsf_Core::post_load()
{
	int chip_flags = header().chip_flags;
	
	#if !NSF_EMU_APU_ONLY
		if ( chip_flags & header_t::fds_mask )
			CHECK_ALLOC( fds = BLARGG_NEW Nes_Fds_Apu );
		
		if ( chip_flags & header_t::fme7_mask )
			CHECK_ALLOC( fme7 = BLARGG_NEW Nes_Fme7_Apu );
		
		if ( chip_flags & header_t::mmc5_mask )
			CHECK_ALLOC( mmc5 = BLARGG_NEW Nes_Mmc5_Apu );
		
		if ( chip_flags & header_t::namco_mask )
			CHECK_ALLOC( namco = BLARGG_NEW Nes_Namco_Apu );
		
		if ( chip_flags & header_t::vrc6_mask )
			CHECK_ALLOC( vrc6 = BLARGG_NEW Nes_Vrc6_Apu );
		
		if ( chip_flags & header_t::vrc7_mask )
		{
			#if NSF_EMU_NO_VRC7
				chip_flags = ~chips_mask; // give warning rather than error
			#else
					CHECK_ALLOC( vrc7 = BLARGG_NEW Nes_Vrc7_Apu );
					RETURN_ERR( vrc7->init() );
			#endif
		}
	#endif

	set_tempo( 1.0 );
	
	if ( chip_flags & ~chips_mask )
		set_warning( "Uses unsupported audio expansion hardware" );

	return Nsf_Impl::post_load();
}

int Nsf_Core::cpu_read( addr_t addr )
{
	#if !NSF_EMU_APU_ONLY
	{
		if ( addr == Nes_Namco_Apu::data_reg_addr && namco )
			return namco->read_data();
		
		if ( (unsigned) (addr - Nes_Fds_Apu::io_addr) < Nes_Fds_Apu::io_size && fds )
			return fds->read( time(), addr );
		
		int i = addr - 0x5C00;
		if ( (unsigned) i < mmc5->exram_size && mmc5 )
			return mmc5->exram [i];
		
		int m = addr - 0x5205;
		if ( (unsigned) m < 2 && mmc5 )
			return (mmc5_mul [0] * mmc5_mul [1]) >> (m * 8) & 0xFF;
	}
	#endif
	
	return Nsf_Impl::cpu_read( addr );
}

int Nsf_Core::unmapped_read( addr_t addr )
{
	switch ( addr )
	{
	case 0x2002:
	case 0x4016:
	case 0x4017:
		return addr >> 8;
	}

	return Nsf_Impl::unmapped_read( addr );
}

void Nsf_Core::cpu_write( addr_t addr, int data )
{
	#if !NSF_EMU_APU_ONLY
	{
		if ( (unsigned) (addr - fds->io_addr) < fds->io_size && fds )
		{
			fds->write( time(), addr, data );
			return;
		}
		
		if ( namco )
		{
			if ( addr == namco->addr_reg_addr )
			{
				namco->write_addr( data );
				return;
			}
			
			if ( addr == namco->data_reg_addr )
			{
				namco->write_data( time(), data );
				return;
			}
		}
		
		if ( vrc6 )
		{
			int reg = addr & (vrc6->addr_step - 1);
			int osc = (unsigned) (addr - vrc6->base_addr) / vrc6->addr_step;
			if ( (unsigned) osc < vrc6->osc_count && (unsigned) reg < vrc6->reg_count )
			{
				vrc6->write_osc( time(), osc, reg, data );
				return;
			}
		}
		
		if ( addr >= fme7->latch_addr && fme7 )
		{
			switch ( addr & fme7->addr_mask )
			{
			case Nes_Fme7_Apu::latch_addr:
				fme7->write_latch( data );
				return;
			
			case Nes_Fme7_Apu::data_addr:
				fme7->write_data( time(), data );
				return;
			}
		}
		
		if ( mmc5 )
		{
			if ( (unsigned) (addr - mmc5->regs_addr) < mmc5->regs_size )
			{
				mmc5->write_register( time(), addr, data );
				return;
			}
			
			int m = addr - 0x5205;
			if ( (unsigned) m < 2 )
			{
				mmc5_mul [m] = data;
				return;
			}
			
			int i = addr - 0x5C00;
			if ( (unsigned) i < mmc5->exram_size )
			{
				mmc5->exram [i] = data;
				return;
			}
		}
		
		if ( vrc7 )
		{
			if ( addr == 0x9010 )
			{
				vrc7->write_reg( data );
				return;
			}
			
			if ( (unsigned) (addr - 0x9028) <= 0x08 )
			{
				vrc7->write_data( time(), data );
				return;
			}
		}
	}
	#endif
	
	return Nsf_Impl::cpu_write( addr, data );
}

void Nsf_Core::unmapped_write( addr_t addr, int data )
{
	switch ( addr )
	{
	case 0x8000: // some write to $8000 and $8001 repeatedly
	case 0x8001:
	case 0x4800: // probably namco sound mistakenly turned on in MCK
	case 0xF800:
	case 0xFFF8: // memory mapper?
		return;
	}
	
	if ( mmc5 && addr == 0x5115 ) return;
	
	// FDS memory
	if ( fds && (unsigned) (addr - 0x8000) < 0x6000 ) return;

	Nsf_Impl::unmapped_write( addr, data );
}

blargg_err_t Nsf_Core::start_track( int track )
{
	#if !NSF_EMU_APU_ONLY
		if ( mmc5 )
		{
			mmc5_mul [0] = 0;
			mmc5_mul [1] = 0;
			memset( mmc5->exram, 0, mmc5->exram_size );
		}
	#endif
	
	#if !NSF_EMU_APU_ONLY
		if ( fds   ) fds  ->reset();
		if ( fme7  ) fme7 ->reset();
		if ( mmc5  ) mmc5 ->reset();
		if ( namco ) namco->reset();
		if ( vrc6  ) vrc6 ->reset();
		if ( vrc7  ) vrc7 ->reset();
	#endif
	
	return Nsf_Impl::start_track( track );
}

void Nsf_Core::end_frame( time_t end )
{
	Nsf_Impl::end_frame( end );
	
	#if !NSF_EMU_APU_ONLY
		if ( fds   ) fds  ->end_frame( end );
		if ( fme7  ) fme7 ->end_frame( end );
		if ( mmc5  ) mmc5 ->end_frame( end );
		if ( namco ) namco->end_frame( end );
		if ( vrc6  ) vrc6 ->end_frame( end );
		if ( vrc7  ) vrc7 ->end_frame( end );
	#endif
}
