// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Sap_Apu.h"

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

int const max_frequency = 12000; // pure waves above this frequency are silenced

static void gen_poly( unsigned mask, int count, byte out [] )
{
	unsigned n = 1;
	do
	{
		int bits = 0;
		int b = 0;
		do
		{
			// implemented using "Galios configuration"
			bits |= (n & 1) << b;
			n = (n >> 1) ^ (mask * (n & 1));
		}
		while ( b++ < 7 );
		*out++ = bits;
	}
	while ( --count );
}

// poly5
int const poly5_len = (1 <<  5) - 1;
unsigned const poly5_mask = (1U << poly5_len) - 1;
unsigned const poly5 = 0x167C6EA1;

inline unsigned run_poly5( unsigned in, int shift )
{
	return (in << shift & poly5_mask) | (in >> (poly5_len - shift));
}

#define POLY_MASK( width, tap1, tap2 ) \
	((1U << (width - 1 - tap1)) | (1U << (width - 1 - tap2)))

Sap_Apu_Impl::Sap_Apu_Impl()
{
	gen_poly( POLY_MASK(  4, 1, 0 ), sizeof poly4,  poly4  );
	gen_poly( POLY_MASK(  9, 5, 0 ), sizeof poly9,  poly9  );
	gen_poly( POLY_MASK( 17, 5, 0 ), sizeof poly17, poly17 );
	
	if ( 0 ) // comment out to recauculate poly5 constant
	{
		byte poly5 [4];
		gen_poly( POLY_MASK(  5, 2, 0 ), sizeof poly5,  poly5  );
		unsigned n = poly5 [3] * 0x1000000 + poly5 [2] * 0x10000 + 
				poly5 [1] * 0x100 + poly5 [0];
		unsigned rev = n & 1;
		for ( int i = 1; i < poly5_len; i++ )
			rev |= (n >> i & 1) << (poly5_len - i);
		dprintf( "poly5: 0x%08lX\n", rev );
	}
}

void Sap_Apu::set_output( Blip_Buffer* b )
{
	for ( int i = 0; i < osc_count; ++i )
		set_output( i, b );
}

Sap_Apu::Sap_Apu()
{
	impl = NULL;
	set_output( NULL );
}

void Sap_Apu::reset( Sap_Apu_Impl* new_impl )
{
	impl      = new_impl;
	last_time = 0;
	poly5_pos = 0;
	poly4_pos = 0;
	polym_pos = 0;
	control   = 0;
	
	for ( int i = 0; i < osc_count; i++ )
		memset( &oscs [i], 0, offsetof (osc_t,output) );
}

inline void Sap_Apu::calc_periods()
{
	 // 15/64 kHz clock
	int divider = 28;
	if ( this->control & 1 )
		divider = 114;
	
	for ( int i = 0; i < osc_count; i++ )
	{
		osc_t* const osc = &oscs [i];
		
		int const osc_reload = osc->regs [0]; // cache
		int period = (osc_reload + 1) * divider;
		static byte const fast_bits [osc_count] = { 1 << 6, 1 << 4, 1 << 5, 1 << 3 };
		if ( this->control & fast_bits [i] )
		{
			period = osc_reload + 4;
			if ( i & 1 )
			{
				period = osc_reload * 0x100 + osc [-1].regs [0] + 7;
				if ( !(this->control & fast_bits [i - 1]) )
					period = (period - 6) * divider;
				
				if ( (osc [-1].regs [1] & 0x1F) > 0x10 )
					dprintf( "Use of slave channel in 16-bit mode not supported\n" );
			}
		}
		osc->period = period;
	}
}

void Sap_Apu::run_until( blip_time_t end_time )
{
	calc_periods();
	Sap_Apu_Impl* const impl = this->impl; // cache
	
	// 17/9-bit poly selection
	byte const* polym = impl->poly17;
	int polym_len = poly17_len;
	if ( this->control & 0x80 )
	{
		polym_len = poly9_len;
		polym = impl->poly9;
	}
	polym_pos %= polym_len;
	
	for ( int i = 0; i < osc_count; i++ )
	{
		osc_t* const osc = &oscs [i];
		blip_time_t time = last_time + osc->delay;
		blip_time_t const period = osc->period;
		
		// output
		Blip_Buffer* output = osc->output;
		if ( output )
		{
			int const osc_control = osc->regs [1]; // cache
			int volume = (osc_control & 0x0F) * 2;
			if ( !volume || osc_control & 0x10 || // silent, DAC mode, or inaudible frequency
					((osc_control & 0xA0) == 0xA0 && period < 1789773 / 2 / max_frequency) )
			{
				if ( !(osc_control & 0x10) )
					volume >>= 1; // inaudible frequency = half volume
				
				int delta = volume - osc->last_amp;
				if ( delta )
				{
					osc->last_amp = volume;
					output->set_modified();
					impl->synth.offset( last_time, delta, output );
				}
				
				// TODO: doesn't maintain high pass flip-flop (very minor issue)
			}
			else
			{
				// high pass
				static byte const hipass_bits [osc_count] = { 1 << 2, 1 << 1, 0, 0 };
				blip_time_t period2 = 0; // unused if no high pass
				blip_time_t time2 = end_time;
				if ( this->control & hipass_bits [i] )
				{
					period2 = osc [2].period;
					time2 = last_time + osc [2].delay;
					if ( osc->invert )
					{
						// trick inner wave loop into inverting output
						osc->last_amp -= volume;
						volume = -volume;
					}
				}
				
				if ( time < end_time || time2 < end_time )
				{
					// poly source
					static byte const poly1 [] = { 0x55, 0x55 }; // square wave
					byte const* poly = poly1;
					int poly_len = 8 * sizeof poly1; // can be just 2 bits, but this is faster
					int poly_pos = osc->phase & 1;
					int poly_inc = 1;
					if ( !(osc_control & 0x20) )
					{
						poly     = polym;
						poly_len = polym_len;
						poly_pos = polym_pos;
						if ( osc_control & 0x40 )
						{
							poly     = impl->poly4;
							poly_len = poly4_len;
							poly_pos = poly4_pos;
						}
						poly_inc = period % poly_len;
						poly_pos = (poly_pos + osc->delay) % poly_len;
					}
					poly_inc -= poly_len; // allows more optimized inner loop below
					
					// square/poly5 wave
					unsigned wave = poly5;
					check( poly5 & 1 ); // low bit is set for pure wave
					int poly5_inc = 0;
					if ( !(osc_control & 0x80) )
					{
						wave = run_poly5( wave, (osc->delay + poly5_pos) % poly5_len );
						poly5_inc = period % poly5_len;
					}
					
					output->set_modified();
					
					// Run wave and high pass interleved with each catching up to the other.
					// Disabled high pass has no performance effect since inner wave loop
					// makes no compromise for high pass, and only runs once in that case.
					int osc_last_amp = osc->last_amp;
					do
					{
						// run high pass
						if ( time2 < time )
						{
							int delta = -osc_last_amp;
							if ( volume < 0 )
								delta += volume;
							if ( delta )
							{
								osc_last_amp += delta - volume;
								volume = -volume;
								impl->synth.offset( time2, delta, output );
							}
						}
						while ( time2 <= time ) // must advance *past* time to avoid hang
							time2 += period2;
						
						// run wave
						blip_time_t end = end_time;
						if ( end > time2 )
							end = time2;
						while ( time < end )
						{
							if ( wave & 1 )
							{
								int amp = volume * (poly [poly_pos >> 3] >> (poly_pos & 7) & 1);
								if ( (poly_pos += poly_inc) < 0 )
									poly_pos += poly_len;
								int delta = amp - osc_last_amp;
								if ( delta )
								{
									osc_last_amp = amp;
									impl->synth.offset( time, delta, output );
								}
							}
							wave = run_poly5( wave, poly5_inc );
							time += period;
						}
					}
					while ( time < end_time || time2 < end_time );
					
					osc->phase = poly_pos;
					osc->last_amp = osc_last_amp;
				}
				
				osc->invert = 0;
				if ( volume < 0 )
				{
					// undo inversion trickery
					osc->last_amp -= volume;
					osc->invert = 1;
				}
			}
		}
		
		// maintain divider
		blip_time_t remain = end_time - time;
		if ( remain > 0 )
		{
			int count = (remain + period - 1) / period;
			osc->phase ^= count;
			time += count * period;
		}
		osc->delay = time - end_time;
	}
	
	// advance polies
	blip_time_t duration = end_time - last_time;
	last_time = end_time;
	poly4_pos = (poly4_pos + duration) % poly4_len;
	poly5_pos = (poly5_pos + duration) % poly5_len;
	polym_pos += duration; // will get %'d on next call
}

void Sap_Apu::write_data( blip_time_t time, int addr, int data )
{
	run_until( time );
	int i = (addr - 0xD200) >> 1;
	if ( (unsigned) i < osc_count )
	{
		oscs [i].regs [addr & 1] = data;
	}
	else if ( addr == 0xD208 )
	{
		control = data;
	}
	else if ( addr == 0xD209 )
	{
		oscs [0].delay = 0;
		oscs [1].delay = 0;
		oscs [2].delay = 0;
		oscs [3].delay = 0;
	}
	/*
	// TODO: are polynomials reset in this case?
	else if ( addr == 0xD20F )
	{
		if ( (data & 3) == 0 )
			polym_pos = 0;
	}
	*/
}

void Sap_Apu::end_frame( blip_time_t end_time )
{
	if ( end_time > last_time )
		run_until( end_time );
	
	last_time -= end_time;
	assert( last_time >= 0 );
}
