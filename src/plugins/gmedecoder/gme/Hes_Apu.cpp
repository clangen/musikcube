// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Hes_Apu.h"

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

bool const center_waves = true; // reduces asymmetry and clamping when starting notes

Hes_Apu::Hes_Apu()
{
	for ( Osc* osc = &oscs [osc_count]; osc != oscs; )
	{
		osc--;
		osc->output  [0] = NULL;
		osc->output  [1] = NULL;
		osc->outputs [0] = NULL;
		osc->outputs [1] = NULL;
		osc->outputs [2] = NULL;
	}
	
	reset();
}

void Hes_Apu::reset()
{
	latch   = 0;
	balance = 0xFF;
	
	for ( Osc* osc = &oscs [osc_count]; osc != oscs; )
	{
		osc--;
		memset( osc, 0, offsetof (Osc,output) );
		osc->lfsr    = 0;
		osc->control = 0x40;
		osc->balance = 0xFF;
	}
	
	// Only last two oscs support noise
	oscs [osc_count - 2].lfsr = 0x200C3; // equivalent to 1 in Fibonacci LFSR
	oscs [osc_count - 1].lfsr = 0x200C3;
}

void Hes_Apu::set_output( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	// Must be silent (all NULL), mono (left and right NULL), or stereo (none NULL)
	require( !center || (center && !left && !right) || (center && left && right) );
	require( (unsigned) i < osc_count ); // fails if you pass invalid osc index
	
	if ( !center || !left || !right )
	{
		left  = center;
		right = center;
	}
	
	Osc& o = oscs [i];
	o.outputs [0] = center;
	o.outputs [1] = left;
	o.outputs [2] = right;
	balance_changed( o );
}

void Hes_Apu::run_osc( Blip_Synth_Fast& syn, Osc& o, blip_time_t end_time )
{
	int vol0 = o.volume [0];
	int vol1 = o.volume [1];
	int dac  = o.dac;
	
	Blip_Buffer* out0 = o.output [0]; // cache often-used values
	Blip_Buffer* out1 = o.output [1];
	if ( !(o.control & 0x80) )
		out0 = NULL;
	
	if ( out0 )
	{
		// Update amplitudes
		if ( out1 )
		{
			int delta = dac * vol1 - o.last_amp [1];
			if ( delta )
			{
				syn.offset( o.last_time, delta, out1 );
				out1->set_modified();
			}
		}
		int delta = dac * vol0 - o.last_amp [0];
		if ( delta )
		{
			syn.offset( o.last_time, delta, out0 );
			out0->set_modified();
		}
		
		// Don't generate if silent
		if ( !(vol0 | vol1) )
			out0 = NULL;
	}
	
	// Generate noise
	int noise = 0;
	if ( o.lfsr )
	{
		noise = o.noise & 0x80;
		
		blip_time_t time = o.last_time + o.noise_delay;
		if ( time < end_time )
		{
            int period = (~o.noise & 0x1F) * 128;
			if ( !period )
                period = 64;
			
			if ( noise && out0 )
			{
				unsigned lfsr = o.lfsr;
				do
				{
					int new_dac = -(lfsr & 1);
					lfsr = (lfsr >> 1) ^ (0x30061 & new_dac);
					
					int delta = (new_dac &= 0x1F) - dac;
					if ( delta )
					{
						dac = new_dac;
						syn.offset( time, delta * vol0, out0 );
						if ( out1 )
							syn.offset( time, delta * vol1, out1 );
					}
					time += period;
				}
				while ( time < end_time );
				
				if ( !lfsr )
				{
					lfsr = 1;
					check( false );
				}
				o.lfsr = lfsr;
				
				out0->set_modified();
				if ( out1 )
					out1->set_modified();
			}
			else
			{
				// Maintain phase when silent
				int count = (end_time - time + period - 1) / period;
				time += count * period;
				
				// not worth it
				//while ( count-- )
				//  o.lfsr = (o.lfsr >> 1) ^ (0x30061 * (o.lfsr & 1));
			}
		}
		o.noise_delay = time - end_time;
	}
	
	// Generate wave
	blip_time_t time = o.last_time + o.delay;
	if ( time < end_time )
	{
		int phase = (o.phase + 1) & 0x1F; // pre-advance for optimal inner loop
        int period = o.period * 2;
		
        if ( period >= 14 && out0 && !((o.control & 0x40) | noise) )
		{
			do
			{
				int new_dac = o.wave [phase];
				phase = (phase + 1) & 0x1F;
				int delta = new_dac - dac;
				if ( delta )
				{
					dac = new_dac;
					syn.offset( time, delta * vol0, out0 );
					if ( out1 )
						syn.offset( time, delta * vol1, out1 );
				}
				time += period;
			}
			while ( time < end_time );
			out0->set_modified();
			if ( out1 )
				out1->set_modified();
		}
		else
		{
			// Maintain phase when silent
			int count = end_time - time;
			if ( !period )
				period = 1;
			count = (count + period - 1) / period;
			
			phase += count; // phase will be masked below
			time  += count * period;
		}
		
		// TODO: Find whether phase increments even when both volumes are zero.
		// CAN'T simply check for out0 being non-NULL, since it could be NULL
		// if channel is muted in player, but still has non-zero volume.
		// City Hunter breaks when this check is removed.
		if ( !(o.control & 0x40) && (vol0 | vol1) )
			o.phase = (phase - 1) & 0x1F; // undo pre-advance
	}
	o.delay = time - end_time;
	check( o.delay >= 0 );
	
	o.last_time = end_time;
	o.dac          = dac;
	o.last_amp [0] = dac * vol0;
	o.last_amp [1] = dac * vol1;
}

void Hes_Apu::balance_changed( Osc& osc )
{
	static short const log_table [32] = { // ~1.5 db per step
		#define ENTRY( factor ) short (factor * amp_range / 31.0 + 0.5)
		ENTRY( 0.000000 ),ENTRY( 0.005524 ),ENTRY( 0.006570 ),ENTRY( 0.007813 ),
		ENTRY( 0.009291 ),ENTRY( 0.011049 ),ENTRY( 0.013139 ),ENTRY( 0.015625 ),
		ENTRY( 0.018581 ),ENTRY( 0.022097 ),ENTRY( 0.026278 ),ENTRY( 0.031250 ),
		ENTRY( 0.037163 ),ENTRY( 0.044194 ),ENTRY( 0.052556 ),ENTRY( 0.062500 ),
		ENTRY( 0.074325 ),ENTRY( 0.088388 ),ENTRY( 0.105112 ),ENTRY( 0.125000 ),
		ENTRY( 0.148651 ),ENTRY( 0.176777 ),ENTRY( 0.210224 ),ENTRY( 0.250000 ),
		ENTRY( 0.297302 ),ENTRY( 0.353553 ),ENTRY( 0.420448 ),ENTRY( 0.500000 ),
		ENTRY( 0.594604 ),ENTRY( 0.707107 ),ENTRY( 0.840896 ),ENTRY( 1.000000 ),
		#undef ENTRY
	};
	
	int vol = (osc.control & 0x1F) - 0x1E * 2;
	
	int left  = vol + (osc.balance >> 3 & 0x1E) + (balance >> 3 & 0x1E);
	if ( left  < 0 ) left  = 0;
	
	int right = vol + (osc.balance << 1 & 0x1E) + (balance << 1 & 0x1E);
	if ( right < 0 ) right = 0;
	
	// optimizing for the common case of being centered also allows easy
	// panning using Effects_Buffer
	
	// Separate balance into center volume and additional on either left or right
	osc.output [0] = osc.outputs [0]; // center
	osc.output [1] = osc.outputs [2]; // right
	int base = log_table [left ];
	int side = log_table [right] - base;
	if ( side < 0 )
	{
		base += side;
		side = -side;
		osc.output [1] = osc.outputs [1]; // left
	}
	
	// Optimize when output is far left, center, or far right
	if ( !base || osc.output [0] == osc.output [1] )
	{
		base += side;
		side = 0;
		osc.output [0] = osc.output [1];
		osc.output [1] = NULL;
		osc.last_amp [1] = 0;
	}
	
	if ( center_waves )
	{
		// TODO: this can leave a non-zero level in a buffer (minor)
		osc.last_amp [0] += (base - osc.volume [0]) * 16;
		osc.last_amp [1] += (side - osc.volume [1]) * 16;
	}
	
	osc.volume [0] = base;
	osc.volume [1] = side;
}

void Hes_Apu::write_data( blip_time_t time, int addr, int data )
{
	if ( addr == 0x800 )
	{
		latch = data & 7;
	}
	else if ( addr == 0x801 )
	{
		if ( balance != data )
		{
			balance = data;
			
			for ( Osc* osc = &oscs [osc_count]; osc != oscs; )
			{
				osc--;
				run_osc( synth, *osc, time );
				balance_changed( *oscs );
			}
		}
	}
	else if ( latch < osc_count )
	{
		Osc& osc = oscs [latch];
		run_osc( synth, osc, time );
		switch ( addr )
		{
		case 0x802:
			osc.period = (osc.period & 0xF00) | data;
			break;
		
		case 0x803:
			osc.period = (osc.period & 0x0FF) | ((data & 0x0F) << 8);
			break;
		
		case 0x804:
			if ( osc.control & 0x40 & ~data )
				osc.phase = 0;
			osc.control = data;
			balance_changed( osc );
			break;
		
		case 0x805:
			osc.balance = data;
			balance_changed( osc );
			break;
		
		case 0x806:
			data &= 0x1F;
			if ( !(osc.control & 0x40) )
			{
				osc.wave [osc.phase] = data;
				osc.phase = (osc.phase + 1) & 0x1F;
			}
			else if ( osc.control & 0x80 )
			{
				osc.dac = data;
			}
			break;
		
		 case 0x807:
		 	osc.noise = data;
		 	break;
		 
		 case 0x809:
		 	if ( !(data & 0x80) && (data & 0x03) != 0 )
		 		dprintf( "HES LFO not supported\n" );
		}
	}
}

void Hes_Apu::end_frame( blip_time_t end_time )
{
	for ( Osc* osc = &oscs [osc_count]; osc != oscs; )
	{
		osc--;
		if ( end_time > osc->last_time )
			run_osc( synth, *osc, end_time );
		osc->last_time -= end_time;
		check( osc->last_time >= 0 );
	}
}
