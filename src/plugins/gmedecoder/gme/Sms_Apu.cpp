// Sms_Snd_Emu $vers. http://www.slack.net/~ant/

#include "Sms_Apu.h"

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

int const noise_osc = 3;

void Sms_Apu::volume( double vol )
{
	vol *= 0.85 / osc_count / 64;
	norm_synth.volume( vol );
	fast_synth.volume( vol );
}

void Sms_Apu::treble_eq( blip_eq_t const& eq )
{
	norm_synth.treble_eq( eq );
	fast_synth.treble_eq( eq );
}

inline int Sms_Apu::calc_output( int i ) const
{
	int flags = ggstereo >> i;
	return (flags >> 3 & 2) | (flags & 1);
}

void Sms_Apu::set_output( int i, Blip_Buffer* center, Blip_Buffer* left, Blip_Buffer* right )
{
	// Must be silent (all NULL), mono (left and right NULL), or stereo (none NULL)
	require( !center || (center && !left && !right) || (center && left && right) );
	require( (unsigned) i < osc_count ); // fails if you pass invalid osc index
	
	if ( center )
	{
		unsigned const divisor = 16384 * 16 * 2;
		min_tone_period = ((unsigned) center->clock_rate() + divisor/2) / divisor;
	}
	
	if ( !center || !left || !right )
	{
		left  = center;
		right = center;
	}
	
	Osc& o = oscs [i];
	o.outputs [0] = NULL;
	o.outputs [1] = right;
	o.outputs [2] = left;
	o.outputs [3] = center;
	o.output = o.outputs [calc_output( i )];
}

void Sms_Apu::set_output( Blip_Buffer* c, Blip_Buffer* l, Blip_Buffer* r )
{
	for ( int i = osc_count; --i >= 0; )
		set_output( i, c, l, r );
}

static inline unsigned fibonacci_to_galois_lfsr( unsigned fibonacci, int width )
{
	unsigned galois = 0;
	while ( --width >= 0 )
	{
		galois = (galois << 1) | (fibonacci & 1);
		fibonacci >>= 1;
	}
	return galois;
}

void Sms_Apu::reset( unsigned feedback, int noise_width )
{
	last_time = 0;
	latch     = 0;
	ggstereo  = 0;
	
	// Calculate noise feedback values
	if ( !feedback || !noise_width )
	{
		feedback    = 0x0009;
		noise_width = 16;
	}
	looped_feedback = 1 << (noise_width - 1);
	noise_feedback  = fibonacci_to_galois_lfsr( feedback, noise_width );
	
	// Reset oscs
	for ( int i = osc_count; --i >= 0; )
	{
		Osc& o = oscs [i];
		o.output   =  NULL;
		o.last_amp =  0;
		o.delay    =  0;
		o.phase    =  0;
		o.period   =  0;
		o.volume   = 15; // silent
	}
	
	oscs [noise_osc].phase = 0x8000;
	write_ggstereo( 0, 0xFF );
}

Sms_Apu::Sms_Apu()
{
	min_tone_period = 7;
	
	// Clear outputs to NULL FIRST
	ggstereo = 0;
	set_output( NULL );
	
	volume( 1.0 );
	reset();
}

void Sms_Apu::run_until( blip_time_t end_time )
{
	require( end_time >= last_time );
	if ( end_time <= last_time )
		return;
	
	// Synthesize each oscillator
	for ( int idx = osc_count; --idx >= 0; )
	{
		Osc& osc = oscs [idx];
		int vol  = 0;
		int amp  = 0;
		
		// Determine what will be generated
		Blip_Buffer* const out = osc.output;
		if ( out )
		{
			// volumes [i] ~= 64 * pow( 1.26, 15 - i ) / pow( 1.26, 15 )
			static unsigned char const volumes [16] = {
				64, 50, 40, 32, 25, 20, 16, 13, 10, 8, 6, 5, 4, 3, 2, 0
			};
			
			vol = volumes [osc.volume];
			amp = (osc.phase & 1) * vol;
			
			// Square freq above 16 kHz yields constant amplitude at half volume
			if ( idx != noise_osc && osc.period < min_tone_period )
			{
				amp = vol >> 1;
				vol = 0;
			}
			
			// Update amplitude
			int delta = amp - osc.last_amp;
			if ( delta )
			{
				osc.last_amp = amp;
				norm_synth.offset( last_time, delta, out );
				out->set_modified();
			}
		}
		
		// Generate wave
		blip_time_t time = last_time + osc.delay;
		if ( time < end_time )
		{
			// Calculate actual period
			int period = osc.period;
			if ( idx == noise_osc )
			{
				period = 0x20 << (period & 3);
				if ( period == 0x100 )
					period = oscs [2].period * 2;
			}
			period *= 0x10;
			if ( !period )
				period = 0x10;
			
			// Maintain phase when silent
			int phase = osc.phase;
			if ( !vol )
			{
				int count = (end_time - time + period - 1) / period;
				time += count * period;
				if ( idx != noise_osc ) // TODO: maintain noise LFSR phase?
					phase ^= count & 1;
			}
			else
			{
				int delta = amp * 2 - vol;
				
				if ( idx != noise_osc )
				{
					// Square
					do
					{
						delta = -delta;
						norm_synth.offset( time, delta, out );
						time += period;
					}
					while ( time < end_time );
					phase = (delta >= 0);
				}
				else
				{
					// Noise
					unsigned const feedback = (osc.period & 4 ? noise_feedback : looped_feedback);
					do
					{
						unsigned changed = phase + 1;
						phase = ((phase & 1) * feedback) ^ (phase >> 1);
						if ( changed & 2 ) // true if bits 0 and 1 differ
						{
							delta = -delta;
							fast_synth.offset_inline( time, delta, out );
						}
						time += period;
					}
					while ( time < end_time );
					check( phase );
				}
				osc.last_amp = (phase & 1) * vol;
				out->set_modified();
			}
			osc.phase = phase;
		}
		osc.delay = time - end_time;
	}
	last_time = end_time;
}

void Sms_Apu::write_ggstereo( blip_time_t time, int data )
{
	require( (unsigned) data <= 0xFF );
	
	run_until( time );
	ggstereo = data;
	
	for ( int i = osc_count; --i >= 0; )
	{
		Osc& osc = oscs [i];
		
		Blip_Buffer* old = osc.output;
		osc.output = osc.outputs [calc_output( i )];
		if ( osc.output != old )
		{
			int delta = -osc.last_amp;
			if ( delta )
			{
				osc.last_amp = 0;
				if ( old )
				{
					old->set_modified();
					fast_synth.offset( last_time, delta, old );
				}
			}
		}
	}
}

void Sms_Apu::write_data( blip_time_t time, int data )
{
	require( (unsigned) data <= 0xFF );
	
	run_until( time );
	
	if ( data & 0x80 )
		latch = data;
	
	// We want the raw values written so our save state format can be
	// as close to hardware as possible and unspecific to any emulator.
	int idx = latch >> 5 & 3;
	Osc& osc = oscs [idx];
	if ( latch & 0x10 )
	{
		osc.volume = data & 0x0F;
	}
	else
	{
		if ( idx == noise_osc )
			osc.phase = 0x8000; // reset noise LFSR
		
		// Replace high 6 bits/low 4 bits of register with data
		int lo = osc.period;
		int hi = data << 4;
		if ( idx == noise_osc || (data & 0x80) )
		{
			hi = lo;
			lo = data;
		}
		osc.period = (hi & 0x3F0) | (lo & 0x00F);
	}
}

void Sms_Apu::end_frame( blip_time_t end_time )
{
	if ( end_time > last_time )
		run_until( end_time );
	
	last_time -= end_time;
	assert( last_time >= 0 );
}

#if SMS_APU_CUSTOM_STATE
	#define REFLECT( x, y ) (save ?       (io->y) = (x) :         (x) = (io->y)          )
#else
	#define REFLECT( x, y ) (save ? set_val( io->y, x ) : (void) ((x) = get_val( io->y )))
	
	static unsigned get_val( byte const p [] )
	{
		return  p [3] * 0x1000000 + p [2] * 0x10000 + p [1] * 0x100 + p [0];
	}

	static void set_val( byte p [], unsigned n )
	{
		p [0] = (byte) (n      );
		p [1] = (byte) (n >>  8);
		p [2] = (byte) (n >> 16);
		p [3] = (byte) (n >> 24);
	}
#endif

inline const char* Sms_Apu::save_load( sms_apu_state_t* io, bool save )
{
	#if !SMS_APU_CUSTOM_STATE
		assert( sizeof (sms_apu_state_t) == 128 );
	#endif
	
	// Format of data, where later format is incompatible with earlier
	int format = io->format0;
	REFLECT( format, format );
	if ( format != io->format0 )
		return "Unsupported sound save state format";
	
	// Version of data, where later versions just add fields to the end
	int version = 0;
	REFLECT( version, version );
	
	REFLECT( latch,    latch    );
	REFLECT( ggstereo, ggstereo );
	
	for ( int i = osc_count; --i >= 0; )
	{
		Osc& osc = oscs [i];
		REFLECT( osc.period, periods [i] );
		REFLECT( osc.volume, volumes [i] );
		REFLECT( osc.delay,  delays  [i] );
		REFLECT( osc.phase,  phases  [i] );
	}
	
	return 0;
}

void Sms_Apu::save_state( sms_apu_state_t* out )
{
	save_load( out, true );
	#if !SMS_APU_CUSTOM_STATE
		memset( out->unused, 0, sizeof out->unused );
	#endif
}

blargg_err_t Sms_Apu::load_state( sms_apu_state_t const& in )
{
	RETURN_ERR( save_load( CONST_CAST(sms_apu_state_t*,&in), false ) );
	write_ggstereo( 0, ggstereo );
	return blargg_ok;
}
