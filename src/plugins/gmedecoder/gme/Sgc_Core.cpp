// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sgc_Core.h"

/* Copyright (C) 2009 Shay Green. This module is free software; you
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

void Sgc_Core::set_tempo( double t )
{
	set_play_period( clock_rate() / (header().rate ? 50 : 60) / t );
}

blargg_err_t Sgc_Core::load_( Data_Reader& dr )
{
	RETURN_ERR( Sgc_Impl::load_( dr ) );
	
	if ( sega_mapping() && fm_apu_.supported() )
		RETURN_ERR( fm_apu_.init( clock_rate(), clock_rate() / 72 ) );
	
	set_tempo( 1.0 );
	return blargg_ok;
}

blargg_err_t Sgc_Core::start_track( int t )
{
	if ( sega_mapping() )
	{
		apu_.reset();
		fm_apu_.reset();
		fm_accessed = false;
	}
	else
	{
		apu_.reset( 0x0003, 15 );
	}
	
	return Sgc_Impl::start_track( t );
}

blargg_err_t Sgc_Core::end_frame( time_t t )
{
	RETURN_ERR( Sgc_Impl::end_frame( t ) );
	apu_.end_frame( t );
	if ( sega_mapping() && fm_accessed )
	{
		if ( fm_apu_.supported() )
			fm_apu_.end_frame( t );
		else
			set_warning( "FM sound not supported" );
	}

	return blargg_ok;
}
	
Sgc_Core::Sgc_Core()
{ }

Sgc_Core::~Sgc_Core()
{ }

void Sgc_Core::cpu_out( time_t time, addr_t addr, int data )
{
	int port = addr & 0xFF;
	
	if ( sega_mapping() )
	{
		switch ( port )
		{
		case 0x06:
			apu_.write_ggstereo( time, data );
			return;
		
		case 0x7E:
		case 0x7F:
			apu_.write_data( time, data ); dprintf( "$7E<-%02X\n", data );
			return;
		
		case 0xF0:
			fm_accessed = true;
			if ( fm_apu_.supported() )
				fm_apu_.write_addr( data );//, dprintf( "$F0<-%02X\n", data );
			return;
		
		case 0xF1:
			fm_accessed = true;
			if ( fm_apu_.supported() )
				fm_apu_.write_data( time, data );//, dprintf( "$F1<-%02X\n", data );
			return;
		}
	}
	else if ( port >= 0xE0 )
	{
		apu_.write_data( time, data );
		return;
	}
	
	Sgc_Impl::cpu_out( time, addr, data );
}
