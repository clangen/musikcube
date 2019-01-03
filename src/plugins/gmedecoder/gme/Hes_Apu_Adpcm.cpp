// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Hes_Apu_Adpcm.h"

/* Copyright (C) 2006-2008 Shay Green. This module is free software; you
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

Hes_Apu_Adpcm::Hes_Apu_Adpcm()
{
	output = NULL;

	memset( &state, 0, sizeof( state ) );

	reset();
}

void Hes_Apu_Adpcm::reset()
{
	last_time = 0;
	next_timer = 0;
	last_amp = 0;

	memset( &state.pcmbuf, 0, sizeof(state.pcmbuf) );
	memset( &state.port, 0, sizeof(state.port) );

	state.ad_sample = 0;
	state.ad_ref_index = 0;

	state.addr = 0;
	state.freq = 0;
	state.writeptr = 0;
	state.readptr = 0;
	state.playflag = 0;
	state.repeatflag = 0;
	state.length = 0;
	state.volume = 0xFF;
	state.fadetimer = 0;
	state.fadecount = 0;
}

void Hes_Apu_Adpcm::set_output( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	// Must be silent (all NULL), mono (left and right NULL), or stereo (none NULL)
	require( !center || (center && !left && !right) || (center && left && right) );
	require( (unsigned) i < osc_count ); // fails if you pass invalid osc index
	
	if ( !center || !left || !right )
	{
		left  = center;
		right = center;
	}
	
	output = center;
}

void Hes_Apu_Adpcm::run_until( blip_time_t end_time )
{
	int volume = state.volume;
	int fadetimer = state.fadetimer;
	int fadecount = state.fadecount;
	int last_time = this->last_time;
	double next_timer = this->next_timer;
	int last_amp = this->last_amp;
	
	Blip_Buffer* output = this->output; // cache often-used values

	while ( state.playflag && last_time < end_time )
	{
		while ( last_time >= next_timer )
		{
			if ( fadetimer )
			{
				if ( fadecount > 0 )
				{
					fadecount--;
					volume = 0xFF * fadecount / fadetimer;
				}
				else if ( fadecount < 0 )
				{
					fadecount++;
					volume = 0xFF - ( 0xFF * fadecount / fadetimer );
				}
			}
            next_timer += 7159.091;
		}
		int amp;
		if ( state.ad_low_nibble )
		{
			amp = adpcm_decode( state.pcmbuf[ state.playptr ] & 0x0F );
			state.ad_low_nibble = false;
			state.playptr++;
			state.playedsamplecount++;
			if ( state.playedsamplecount == state.playlength )
			{
				state.playflag = 0;
			}
		}
		else
		{
			amp = adpcm_decode( state.pcmbuf[ state.playptr ] >> 4 );
			state.ad_low_nibble = true;
		}
		amp = amp * volume / 0xFF;
		int delta = amp - last_amp;
		if ( output && delta )
		{
			last_amp = amp;
			synth.offset_inline( last_time, delta, output );
		}
		last_time += state.freq;
	}

	if ( !state.playflag )
	{
        while ( next_timer <= end_time ) next_timer += 7159.091;
		last_time = end_time;
	}
	
	this->last_time  = last_time;
	this->next_timer = next_timer;
	this->last_amp   = last_amp;
	state.volume = volume;
	state.fadetimer = fadetimer;
	state.fadecount = fadecount;
}

void Hes_Apu_Adpcm::write_data( blip_time_t time, int addr, int data )
{
	if ( time > last_time ) run_until( time );

	data &= 0xFF;
	state.port[ addr & 15 ] = data;
	switch ( addr & 15 )
	{
	case 8:
		state.addr &= 0xFF00;
		state.addr |= data;
		break;
	case 9:
		state.addr &= 0xFF;
		state.addr |= data << 8;
		break;
	case 10:
		state.pcmbuf[ state.writeptr++ ] = data;
		state.playlength ++;
		break;
	case 11:
		dprintf("ADPCM DMA 0x%02X", data);
		break;
	case 13:
		if ( data & 0x80 )
		{
			state.addr = 0;
			state.freq = 0;
			state.writeptr = 0;
			state.readptr = 0;
			state.playflag = 0;
			state.repeatflag = 0;
			state.length = 0;
			state.volume = 0xFF;
		}
		if ( ( data & 3 ) == 3 )
		{
			state.writeptr = state.addr;
		}
		if ( data & 8 )
		{
			state.readptr = state.addr ? state.addr - 1 : state.addr;
		}
		if ( data & 0x10 )
		{
			state.length = state.addr;
		}
		state.repeatflag = data & 0x20;
		state.playflag = data & 0x40;
		if ( state.playflag )
		{
			state.playptr = state.readptr;
			state.playlength = state.length + 1;
			state.playedsamplecount = 0;
			state.ad_sample = 0;
			state.ad_low_nibble = false;
		}
		break;
	case 14:
        state.freq = 7159091 / ( 32000 / ( 16 - ( data & 15 ) ) );
		break;
	case 15:
		switch ( data & 15 )
		{
		case 0:
		case 8:
		case 12:
			state.fadetimer = -100;
			state.fadecount = state.fadetimer;
			break;
		case 10:
			state.fadetimer = 5000;
			state.fadecount = state.fadetimer;
			break;
		case 14:
			state.fadetimer = 1500;
			state.fadecount = state.fadetimer;
			break;
		}
		break;
	}
}

int Hes_Apu_Adpcm::read_data( blip_time_t time, int addr )
{
	if ( time > last_time ) run_until( time );

	switch ( addr & 15 )
	{
	case 10:
		return state.pcmbuf [state.readptr++];
	case 11:
		return state.port [11] & ~1;
	case 12:
		if (!state.playflag)
		{
			state.port [12] |= 1;
			state.port [12] &= ~8;
		}
		else
		{
			state.port [12] &= ~1;
			state.port [12] |= 8;
		}
		return state.port [12];
	case 13:
		return state.port [13];
	}

	return 0xFF;
}

void Hes_Apu_Adpcm::end_frame( blip_time_t end_time )
{
	run_until( end_time );
	last_time -= end_time;
	next_timer -= (double)end_time;
	check( last_time >= 0 );
	if ( output )
		output->set_modified();
}

static short stepsize[49] = {
  16,  17,  19,  21,  23,  25,  28,
  31,  34,  37,  41,  45,  50,  55,
  60,  66,  73,  80,  88,  97, 107,
 118, 130, 143, 157, 173, 190, 209,
 230, 253, 279, 307, 337, 371, 408,
 449, 494, 544, 598, 658, 724, 796,
 876, 963,1060,1166,1282,1411,1552
};

int Hes_Apu_Adpcm::adpcm_decode( int code )
{
	int step = stepsize[state.ad_ref_index];
	int delta;
	int c = code & 7;
#if 1
	delta = 0;
	if ( c & 4 ) delta += step;
	step >>= 1;
	if ( c & 2 ) delta += step;
	step >>= 1;
	if ( c & 1 ) delta += step;
	step >>= 1;
	delta += step;
#else
	delta = ( ( c + c + 1 ) * step ) / 8; // maybe faster, but introduces rounding
#endif
	if ( c != code )
	{
		state.ad_sample -= delta;
		if ( state.ad_sample < -2048 )
			state.ad_sample = -2048;
	}
	else
	{
		state.ad_sample += delta;
		if ( state.ad_sample > 2047 )
			state.ad_sample = 2047;
	}

	static int const steps [8] = {
		-1, -1, -1, -1, 2, 4, 6, 8
	};
	state.ad_ref_index += steps [c];
	if ( state.ad_ref_index < 0 )
		state.ad_ref_index = 0;
	else if ( state.ad_ref_index > 48 )
		state.ad_ref_index = 48;

	return state.ad_sample;
}
