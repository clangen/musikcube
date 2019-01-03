// $package. http://www.slack.net/~ant/

#include "Ay_Apu.h"

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

// Emulation inaccuracies:
// * Noise isn't run when not in use
// * Changes to envelope and noise periods are delayed until next reload
// * Super-sonic tone should attenuate output to about 60%, not 50%

// Tones above this frequency are treated as disabled tone at half volume.
// Power of two is more efficient (avoids division).
int const inaudible_freq = 16384;

int const period_factor = 16;

static byte const amp_table [16] =
{
#define ENTRY( n ) byte (n * Ay_Apu::amp_range + 0.5)
	// With channels tied together and 1K resistor to ground (as datasheet recommends),
	// output nearly matches logarithmic curve as claimed. Approx. 1.5 dB per step.
	ENTRY(0.000000),ENTRY(0.007813),ENTRY(0.011049),ENTRY(0.015625),
	ENTRY(0.022097),ENTRY(0.031250),ENTRY(0.044194),ENTRY(0.062500),
	ENTRY(0.088388),ENTRY(0.125000),ENTRY(0.176777),ENTRY(0.250000),
	ENTRY(0.353553),ENTRY(0.500000),ENTRY(0.707107),ENTRY(1.000000),
	
	/*
	// Measured from an AY-3-8910A chip with date code 8611.
	
	// Direct voltages without any load (very linear)
	ENTRY(0.000000),ENTRY(0.046237),ENTRY(0.064516),ENTRY(0.089785),
	ENTRY(0.124731),ENTRY(0.173118),ENTRY(0.225806),ENTRY(0.329032),
	ENTRY(0.360215),ENTRY(0.494624),ENTRY(0.594624),ENTRY(0.672043),
	ENTRY(0.766129),ENTRY(0.841935),ENTRY(0.926882),ENTRY(1.000000),
	// With only some load
	ENTRY(0.000000),ENTRY(0.011940),ENTRY(0.017413),ENTRY(0.024876),
	ENTRY(0.036318),ENTRY(0.054229),ENTRY(0.072637),ENTRY(0.122388),
	ENTRY(0.174129),ENTRY(0.239303),ENTRY(0.323881),ENTRY(0.410945),
	ENTRY(0.527363),ENTRY(0.651741),ENTRY(0.832338),ENTRY(1.000000),
	*/
#undef ENTRY
};

static byte const modes [8] =
{
#define MODE( a0,a1, b0,b1, c0,c1 ) \
		(a0 | a1<<1 | b0<<2 | b1<<3 | c0<<4 | c1<<5)
	MODE( 1,0, 1,0, 1,0 ),
	MODE( 1,0, 0,0, 0,0 ),
	MODE( 1,0, 0,1, 1,0 ),
	MODE( 1,0, 1,1, 1,1 ),
	MODE( 0,1, 0,1, 0,1 ),
	MODE( 0,1, 1,1, 1,1 ),
	MODE( 0,1, 1,0, 0,1 ),
	MODE( 0,1, 0,0, 0,0 ),
};

void Ay_Apu::set_output( Blip_Buffer* b )
{
	for ( int i = 0; i < osc_count; ++i )
		set_output( i, b );
}

Ay_Apu::Ay_Apu()
{
	// build full table of the upper 8 envelope waveforms
	for ( int m = 8; m--; )
	{
		byte* out = env_modes [m];
		int flags = modes [m];
		for ( int x = 3; --x >= 0; )
		{
			int amp = flags & 1;
			int end = flags >> 1 & 1;
			int step = end - amp;
			amp *= 15;
			for ( int y = 16; --y >= 0; )
			{
				*out++ = amp_table [amp];
				amp += step;
			}
			flags >>= 2;
		}
	}
	
	type_ = Ay8910;
	set_output( NULL );
	volume( 1.0 );
	reset();
}

void Ay_Apu::reset()
{
	addr_       = 0;
	last_time   = 0;
	noise_delay = 0;
    noise_lfsr  = 1;

	for ( osc_t* osc = &oscs [osc_count]; osc != oscs; )
	{
		osc--;
		osc->period   = period_factor;
        osc->delay    = 0;
		osc->last_amp = 0;
		osc->phase    = 0;
	}
	
	for ( int i = sizeof regs; --i >= 0; )
		regs [i] = 0;
	regs [7] = 0xFF;
	write_data_( 13, 0 );
}

int Ay_Apu::read()
{
	static byte const masks [reg_count] = { 
		0xFF, 0x0F, 0xFF, 0x0F, 0xFF, 0x0F, 0x1F, 0x3F,
		0x1F, 0x1F, 0x1F, 0xFF, 0xFF, 0x0F, 0x00, 0x00
	};
	if (!(type_ & 0x10)) return regs [addr_] & masks [addr_];
	else return regs [addr_];
}

void Ay_Apu::write_data_( int addr, int data )
{
	assert( (unsigned) addr < reg_count );
	
	if ( (unsigned) addr >= 14 )
		dprintf( "Wrote to I/O port %02X\n", (int) addr );
	
	// envelope mode
	if ( addr == 13 )
	{
		if ( !(data & 8) ) // convert modes 0-7 to proper equivalents
			data = (data & 4) ? 15 : 9;
		env_wave = env_modes [data - 7];
		env_pos = -48;
		env_delay = 0; // will get set to envelope period in run_until()
	}
	regs [addr] = data;
	
	// handle period changes accurately
	int i = addr >> 1;
	if ( i < osc_count )
	{
		blip_time_t period = (regs [i * 2 + 1] & 0x0F) * (0x100 * period_factor) +
				regs [i * 2] * period_factor;
		if ( !period )
			period = period_factor;
		
		// adjust time of next timer expiration based on change in period
		osc_t& osc = oscs [i];
		if ( (osc.delay += period - osc.period) < 0 )
			osc.delay = 0;
		osc.period = period;
	}
	
	// TODO: same as above for envelope timer, and it also has a divide by two after it
}

int const noise_off = 0x08;
int const tone_off  = 0x01;

void Ay_Apu::run_until( blip_time_t final_end_time )
{
	require( final_end_time >= last_time );
	
	// noise period and initial values
	blip_time_t const noise_period_factor = period_factor * 2; // verified
	blip_time_t noise_period = (regs [6] & 0x1F) * noise_period_factor;
	if ( !noise_period )
		noise_period = noise_period_factor;
	blip_time_t const old_noise_delay = noise_delay;
	unsigned const old_noise_lfsr = noise_lfsr;
	
	// envelope period
    int env_step_scale = ((type_ & 0xF0) == 0x00) ? 1 : 0;
	blip_time_t const env_period_factor = period_factor << env_step_scale; // verified
	blip_time_t env_period = (regs [12] * 0x100 + regs [11]) * env_period_factor;
	if ( !env_period )
		env_period = env_period_factor; // same as period 1 on my AY chip
	if ( !env_delay )
        env_delay = env_period;
	
	// run each osc separately
	for ( int index = 0; index < osc_count; index++ )
	{
		osc_t* const osc = &oscs [index];
        int osc_mode = regs [7] >> index;
		
		// output
		Blip_Buffer* const osc_output = osc->output;
		if ( !osc_output )
			continue;
		osc_output->set_modified();
		
		// period
		int half_vol = 0;
		blip_time_t inaudible_period = (unsigned) (osc_output->clock_rate() +
				inaudible_freq) / (unsigned) (inaudible_freq * 2);
		if ( osc->period <= inaudible_period && !(osc_mode & tone_off) )
		{
			half_vol = 1; // Actually around 60%, but 50% is close enough
			osc_mode |= tone_off;
		}
		
		// envelope
		blip_time_t start_time = last_time;
		blip_time_t end_time   = final_end_time;
        int const vol_mode = regs [0x08 + index];
		int const vol_mode_mask = type_ == Ay8914 ? 0x30 : 0x10;
		int volume = amp_table [vol_mode & 0x0F] >> (half_vol + env_step_scale);
		int osc_env_pos = env_pos;
		if ( vol_mode & vol_mode_mask )
		{
			volume = env_wave [osc_env_pos] >> (half_vol + env_step_scale);
			if ( type_ == Ay8914 ) volume >>= 3 - ( ( vol_mode & vol_mode_mask ) >> 4 );
			// use envelope only if it's a repeating wave or a ramp that hasn't finished
			if ( !(regs [13] & 1) || osc_env_pos < -32 )
			{
				end_time = start_time + env_delay;
				if ( end_time >= final_end_time )
					end_time = final_end_time;
				
				//if ( !(regs [12] | regs [11]) )
				//  dprintf( "Used envelope period 0\n" );
			}
			else if ( !volume )
			{
				osc_mode = noise_off | tone_off;
			}
		}
		else if ( !volume )
		{
			osc_mode = noise_off | tone_off;
		}
		
		// tone time
		blip_time_t const period = osc->period;
		blip_time_t time = start_time + osc->delay;
		if ( osc_mode & tone_off ) // maintain tone's phase when off
		{
			int count = (final_end_time - time + period - 1) / period;
			time += count * period;
			osc->phase ^= count & 1;
		}
		
		// noise time
		blip_time_t ntime = final_end_time;
		unsigned noise_lfsr = 1;
		if ( !(osc_mode & noise_off) )
		{
			ntime = start_time + old_noise_delay;
			noise_lfsr = old_noise_lfsr;
			//if ( (regs [6] & 0x1F) == 0 )
			//  dprintf( "Used noise period 0\n" );
		}
		
		// The following efficiently handles several cases (least demanding first):
		// * Tone, noise, and envelope disabled, where channel acts as 4-bit DAC
		// * Just tone or just noise, envelope disabled
		// * Envelope controlling tone and/or noise
		// * Tone and noise disabled, envelope enabled with high frequency
		// * Tone and noise together
		// * Tone and noise together with envelope
		
		// This loop only runs one iteration if envelope is disabled. If envelope
		// is being used as a waveform (tone and noise disabled), this loop will
		// still be reasonably efficient since the bulk of it will be skipped.
		while ( 1 )
		{
			// current amplitude
			int amp = 0;
			if ( (osc_mode | osc->phase) & 1 & (osc_mode >> 3 | noise_lfsr) )
				amp = volume;
			{
				int delta = amp - osc->last_amp;
				if ( delta )
				{
					osc->last_amp = amp;
					synth_.offset( start_time, delta, osc_output );
				}
			}
			
			// Run wave and noise interleved with each catching up to the other.
			// If one or both are disabled, their "current time" will be past end time,
			// so there will be no significant performance hit.
			if ( ntime < end_time || time < end_time )
			{
				// Since amplitude was updated above, delta will always be +/- volume,
				// so we can avoid using last_amp every time to calculate the delta.
				int delta = amp * 2 - volume;
				int delta_non_zero = delta != 0;
				int phase = osc->phase | (osc_mode & tone_off); assert( tone_off == 0x01 );
				do
				{
					// run noise
					blip_time_t end = end_time;
					if ( end_time > time ) end = time;
					if ( phase & delta_non_zero )
					{
						while ( ntime <= end ) // must advance *past* time to avoid hang
						{
							int changed = noise_lfsr + 1;
							noise_lfsr = (-(noise_lfsr & 1) & 0x12000) ^ (noise_lfsr >> 1);
							if ( changed & 2 )
							{
								delta = -delta;
								synth_.offset( ntime, delta, osc_output );
							}
							ntime += noise_period;
						}
					}
					else
					{
						// 20 or more noise periods on average for some music
						int remain = end - ntime;
						int count = remain / noise_period;
						if ( remain >= 0 )
							ntime += noise_period + count * noise_period;
					}
					
					// run tone
					end = end_time;
					if ( end_time > ntime ) end = ntime;
					if ( noise_lfsr & delta_non_zero )
					{
						while ( time < end )
						{
							delta = -delta;
							synth_.offset( time, delta, osc_output );
							time += period;
							
							// alternate (less-efficient) implementation
							//phase ^= 1;
						}
						phase = unsigned (-delta) >> (CHAR_BIT * sizeof (unsigned) - 1);
						check( phase == (delta > 0) );
					}
					else
					{
						// loop usually runs less than once
						//SUB_CASE_COUNTER( (time < end) * (end - time + period - 1) / period );
						
						while ( time < end )
						{
							time += period;
							phase ^= 1;
						}
					}
				}
				while ( time < end_time || ntime < end_time );
				
				osc->last_amp = (delta + volume) >> 1;
				if ( !(osc_mode & tone_off) )
					osc->phase = phase;
			}
			
			if ( end_time >= final_end_time )
				break; // breaks first time when envelope is disabled
			
			// next envelope step
			if ( ++osc_env_pos >= 0 )
				osc_env_pos -= 32;
			volume = env_wave [osc_env_pos] >> (half_vol + env_step_scale);
			if ( type_ == Ay8914 ) volume >>= 3 - ( ( vol_mode & vol_mode_mask ) >> 4 );
			
			start_time = end_time;
            end_time += env_period;
			if ( end_time > final_end_time )
				end_time = final_end_time;
		}
		osc->delay = time - final_end_time;
		
		if ( !(osc_mode & noise_off) )
		{
			noise_delay = ntime - final_end_time;
			this->noise_lfsr = noise_lfsr;
		}
	}
	
	// TODO: optimized saw wave envelope?
	
	// maintain envelope phase
	blip_time_t remain = final_end_time - last_time - env_delay;
	if ( remain >= 0 )
	{
		int count = (remain + env_period) / env_period;
		env_pos += count;
		if ( env_pos >= 0 )
			env_pos = (env_pos & 31) - 32;
		remain -= count * env_period;
		assert( -remain <= env_period );
	}
	env_delay = -remain;
	assert( env_delay > 0 );
	assert( env_pos < 0 );
	
	last_time = final_end_time;
}
