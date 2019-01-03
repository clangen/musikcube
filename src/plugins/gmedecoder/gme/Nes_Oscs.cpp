// Nes_Snd_Emu $vers. http://www.slack.net/~ant/

#include "Nes_Apu.h"

/* Copyright (C) 2003-2006 Shay Green. This module is free software; you
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

// Nes_Osc

void Nes_Osc::clock_length( int halt_mask )
{
	if ( length_counter && !(regs [0] & halt_mask) )
		length_counter--;
}

void Nes_Envelope::clock_envelope()
{
	int period = regs [0] & 15;
	if ( reg_written [3] )
	{
		reg_written [3] = false;
		env_delay = period;
		envelope = 15;
	}
	else if ( --env_delay < 0 )
	{
		env_delay = period;
		if ( envelope | (regs [0] & 0x20) )
			envelope = (envelope - 1) & 15;
	}
}

int Nes_Envelope::volume() const
{
	return length_counter == 0 ? 0 : (regs [0] & 0x10) ? (regs [0] & 15) : envelope;
}

// Nes_Square

void Nes_Square::clock_sweep( int negative_adjust )
{
	int sweep = regs [1];
	
	if ( --sweep_delay < 0 )
	{
		reg_written [1] = true;
		
		int period = this->period();
		int shift = sweep & shift_mask;
		if ( shift && (sweep & 0x80) && period >= 8 )
		{
			int offset = period >> shift;
			
			if ( sweep & negate_flag )
				offset = negative_adjust - offset;
			
			if ( period + offset < 0x800 )
			{
				period += offset;
				// rewrite period
				regs [2] = period & 0xFF;
				regs [3] = (regs [3] & ~7) | ((period >> 8) & 7);
			}
		}
	}
	
	if ( reg_written [1] )
	{
		reg_written [1] = false;
		sweep_delay = (sweep >> 4) & 7;
	}
}

// TODO: clean up
inline Nes_Square::nes_time_t Nes_Square::maintain_phase( nes_time_t time, nes_time_t end_time,
		nes_time_t timer_period )
{
	nes_time_t remain = end_time - time;
	if ( remain > 0 )
	{
		int count = (remain + timer_period - 1) / timer_period;
		phase = (phase + count) & (phase_range - 1);
		time += count * timer_period;
	}
	return time;
}

void Nes_Square::run( nes_time_t time, nes_time_t end_time )
{
	const int period = this->period();
	const int timer_period = (period + 1) * 2;
	
	if ( !output )
	{
		delay = maintain_phase( time + delay, end_time, timer_period ) - end_time;
		return;
	}
	
	int offset = period >> (regs [1] & shift_mask);
	if ( regs [1] & negate_flag )
		offset = 0;
	
	const int volume = this->volume();
	if ( volume == 0 || period < 8 || (period + offset) >= 0x800 )
	{
		if ( last_amp )
		{
			output->set_modified();
			synth.offset( time, -last_amp, output );
			last_amp = 0;
		}
		
		time += delay;
		time = maintain_phase( time, end_time, timer_period );
	}
	else
	{
		// handle duty select
		int duty_select = (regs [0] >> 6) & 3;
		int duty = 1 << duty_select; // 1, 2, 4, 2
		int amp = 0;
		if ( duty_select == 3 )
		{
			duty = 2; // negated 25%
			amp = volume;
		}
		if ( phase < duty )
			amp ^= volume;
		
		output->set_modified();
		{
			int delta = update_amp( amp );
			if ( delta )
				synth.offset( time, delta, output );
		}
		
		time += delay;
		if ( time < end_time )
		{
			Blip_Buffer* const output = this->output;
			const Synth& synth = this->synth;
			int delta = amp * 2 - volume;
			int phase = this->phase;
			
			do
			{
				phase = (phase + 1) & (phase_range - 1);
				if ( phase == 0 || phase == duty )
				{
					delta = -delta;
					synth.offset_inline( time, delta, output );
				}
				time += timer_period;
			}
			while ( time < end_time );
			
			last_amp = (delta + volume) >> 1;
			this->phase = phase;
		}
	}
	
	delay = time - end_time;
}

// Nes_Triangle

void Nes_Triangle::clock_linear_counter()
{
	if ( reg_written [3] )
		linear_counter = regs [0] & 0x7F;
	else if ( linear_counter )
		linear_counter--;
	
	if ( !(regs [0] & 0x80) )
		reg_written [3] = false;
}

inline int Nes_Triangle::calc_amp() const
{
	int amp = phase_range - phase;
	if ( amp < 0 )
		amp = phase - (phase_range + 1);
	return amp;
}

// TODO: clean up
inline Nes_Square::nes_time_t Nes_Triangle::maintain_phase( nes_time_t time, nes_time_t end_time,
		nes_time_t timer_period )
{
	nes_time_t remain = end_time - time;
	if ( remain > 0 )
	{
		int count = (remain + timer_period - 1) / timer_period;
		phase = ((unsigned) phase + 1 - count) & (phase_range * 2 - 1);
		phase++;
		time += count * timer_period;
	}
	return time;
}

void Nes_Triangle::run( nes_time_t time, nes_time_t end_time )
{
	const int timer_period = period() + 1;
	if ( !output )
	{
		time += delay;
		delay = 0;
		if ( length_counter && linear_counter && timer_period >= 3 )
			delay = maintain_phase( time, end_time, timer_period ) - end_time;
		return;
	}
	
	// to do: track phase when period < 3
	// to do: Output 7.5 on dac when period < 2? More accurate, but results in more clicks.
	
	int delta = update_amp( calc_amp() );
	if ( delta )
	{
		output->set_modified();
		synth.offset( time, delta, output );
	}
	
	time += delay;
	if ( length_counter == 0 || linear_counter == 0 || timer_period < 3 )
	{
		time = end_time;
	}
	else if ( time < end_time )
	{
		Blip_Buffer* const output = this->output;
		
		int phase = this->phase;
		int volume = 1;
		if ( phase > phase_range )
		{
			phase -= phase_range;
			volume = -volume;
		}
		output->set_modified();
		
		do
		{
			if ( --phase == 0 )
			{
				phase = phase_range;
				volume = -volume;
			}
			else
			{
				synth.offset_inline( time, volume, output );
			}
			
			time += timer_period;
		}
		while ( time < end_time );
		
		if ( volume < 0 )
			phase += phase_range;
		this->phase = phase;
		last_amp = calc_amp();
 	}
	delay = time - end_time;
}

// Nes_Dmc

void Nes_Dmc::reset()
{
	address = 0;
	dac = 0;
	buf = 0;
	bits_remain = 1;
	bits = 0;
	buf_full = false;
	silence = true;
	next_irq = Nes_Apu::no_irq;
	irq_flag = false;
	irq_enabled = false;
	
	Nes_Osc::reset();
	period = 0x1AC;
}

void Nes_Dmc::recalc_irq()
{
	nes_time_t irq = Nes_Apu::no_irq;
	if ( irq_enabled && length_counter )
		irq = apu->last_dmc_time + delay +
				((length_counter - 1) * 8 + bits_remain - 1) * nes_time_t (period) + 1;
	if ( irq != next_irq )
	{
		next_irq = irq;
		apu->irq_changed();
	}
}

int Nes_Dmc::count_reads( nes_time_t time, nes_time_t* last_read ) const
{
	if ( last_read )
		*last_read = time;
	
	if ( length_counter == 0 )
		return 0; // not reading
	
	nes_time_t first_read = next_read_time();
	nes_time_t avail = time - first_read;
	if ( avail <= 0 )
		return 0;
	
	int count = (avail - 1) / (period * 8) + 1;
	if ( !(regs [0] & loop_flag) && count > length_counter )
		count = length_counter;
	
	if ( last_read )
	{
		*last_read = first_read + (count - 1) * (period * 8) + 1;
		check( *last_read <= time );
		check( count == count_reads( *last_read, NULL ) );
		check( count - 1 == count_reads( *last_read - 1, NULL ) );
	}
	
	return count;
}

static short const dmc_period_table [2] [16] = {
	{428, 380, 340, 320, 286, 254, 226, 214, // NTSC
	190, 160, 142, 128, 106,  84,  72,  54},

	{398, 354, 316, 298, 276, 236, 210, 198, // PAL
	176, 148, 132, 118,  98,  78,  66,  50}
};

inline void Nes_Dmc::reload_sample()
{
	address = 0x4000 + regs [2] * 0x40;
	length_counter = regs [3] * 0x10 + 1;
}

static int const dmc_table [128] =
{
   0,  24,  48,  71,  94, 118, 141, 163, 186, 209, 231, 253, 275, 297, 319, 340,
 361, 383, 404, 425, 445, 466, 486, 507, 527, 547, 567, 587, 606, 626, 645, 664,
 683, 702, 721, 740, 758, 777, 795, 813, 832, 850, 867, 885, 903, 920, 938, 955,
 972, 989,1006,1023,1040,1056,1073,1089,1105,1122,1138,1154,1170,1185,1201,1217,
1232,1248,1263,1278,1293,1308,1323,1338,1353,1368,1382,1397,1411,1425,1440,1454,
1468,1482,1496,1510,1523,1537,1551,1564,1578,1591,1604,1618,1631,1644,1657,1670,
1683,1695,1708,1721,1733,1746,1758,1771,1783,1795,1807,1819,1831,1843,1855,1867,
1879,1890,1902,1914,1925,1937,1948,1959,1971,1982,1993,2004,2015,2026,2037,2048,
};

inline int Nes_Dmc::update_amp_nonlinear( int in )
{
	if ( !nonlinear )
		in = dmc_table [in];
	int delta = in - last_amp;
	last_amp = in;
	return delta;
}

void Nes_Dmc::write_register( int addr, int data )
{
	if ( addr == 0 )
	{
		period = dmc_period_table [pal_mode] [data & 15];
		irq_enabled = (data & 0xC0) == 0x80; // enabled only if loop disabled
		irq_flag &= irq_enabled;
		recalc_irq();
	}
	else if ( addr == 1 )
	{
		dac = data & 0x7F;
	}
}

void Nes_Dmc::start()
{
	reload_sample();
	fill_buffer();
	recalc_irq();
}

void Nes_Dmc::fill_buffer()
{
	if ( !buf_full && length_counter )
	{
		require( apu->dmc_reader.f ); // dmc_reader must be set
		buf = apu->dmc_reader.f( apu->dmc_reader.data, 0x8000u + address );
		address = (address + 1) & 0x7FFF;
		buf_full = true;
		if ( --length_counter == 0 )
		{
			if ( regs [0] & loop_flag )
			{
				reload_sample();
			}
			else
			{
				apu->osc_enables &= ~0x10;
				irq_flag = irq_enabled;
				next_irq = Nes_Apu::no_irq;
				apu->irq_changed();
			}
		}
	}
}

void Nes_Dmc::run( nes_time_t time, nes_time_t end_time )
{
	int delta = update_amp_nonlinear( dac );
	if ( !output )
	{
		silence = true;
	}
	else if ( delta )
	{
		output->set_modified();
		synth.offset( time, delta, output );
	}
	
	time += delay;
	if ( time < end_time )
	{
		int bits_remain = this->bits_remain;
		if ( silence && !buf_full )
		{
			int count = (end_time - time + period - 1) / period;
			bits_remain = (bits_remain - 1 + 8 - (count % 8)) % 8 + 1;
			time += count * period;
		}
		else
		{
			Blip_Buffer* const output = this->output;
			const int period = this->period;
			int bits = this->bits;
			int dac = this->dac;
			if ( output )
				output->set_modified();
			
			do
			{
				if ( !silence )
				{
					int step = (bits & 1) * 4 - 2;
					bits >>= 1;
					if ( unsigned (dac + step) <= 0x7F )
					{
						dac += step;
						synth.offset_inline( time, update_amp_nonlinear( dac ), output );
					}
				}
				
				time += period;
				
				if ( --bits_remain == 0 )
				{
					bits_remain = 8;
					if ( !buf_full )
					{
						silence = true;
					}
					else
					{
						silence = false;
						bits = buf;
						buf_full = false;
						if ( !output )
							silence = true;
						fill_buffer();
					}
				}
			}
			while ( time < end_time );
			
			this->dac = dac;
			this->bits = bits;
		}
		this->bits_remain = bits_remain;
	}
	delay = time - end_time;
}

// Nes_Noise

static short const noise_period_table [16] = {
	0x004, 0x008, 0x010, 0x020, 0x040, 0x060, 0x080, 0x0A0,
	0x0CA, 0x0FE, 0x17C, 0x1FC, 0x2FA, 0x3F8, 0x7F2, 0xFE4
};

void Nes_Noise::run( nes_time_t time, nes_time_t end_time )
{
	int period = noise_period_table [regs [2] & 15];
	
	if ( !output )
	{
		// TODO: clean up
		time += delay;
		delay = time + (end_time - time + period - 1) / period * period - end_time;
		return;
	}
	
	
	const int volume = this->volume();
	int amp = (noise & 1) ? volume : 0;
	{
		int delta = update_amp( amp );
		if ( delta )
		{
			output->set_modified();
			synth.offset( time, delta, output );
		}
	}
	
	time += delay;
	if ( time < end_time )
	{
		const int mode_flag = 0x80;
		
		if ( !volume )
		{
			// round to next multiple of period
			time += (end_time - time + period - 1) / period * period;
			
			// approximate noise cycling while muted, by shuffling up noise register
			// to do: precise muted noise cycling?
			if ( !(regs [2] & mode_flag) )
			{
				int feedback = (noise << 13) ^ (noise << 14);
				noise = (feedback & 0x4000) | (noise >> 1);
			}
		}
		else
		{
			Blip_Buffer* const output = this->output;
			
			// using resampled time avoids conversion in synth.offset()
			blip_resampled_time_t rperiod = output->resampled_duration( period );
			blip_resampled_time_t rtime = output->resampled_time( time );
			
			int noise = this->noise;
			int delta = amp * 2 - volume;
			const int tap = (regs [2] & mode_flag ? 8 : 13);
			output->set_modified();
			
			do
			{
				int feedback = (noise << tap) ^ (noise << 14);
				time += period;
				
				if ( (noise + 1) & 2 )
				{
					// bits 0 and 1 of noise differ
					delta = -delta;
					synth.offset_resampled( rtime, delta, output );
				}
				
				rtime += rperiod;
				noise = (feedback & 0x4000) | (noise >> 1);
			}
			while ( time < end_time );
			
			last_amp = (delta + volume) >> 1;
			this->noise = noise;
		}
	}
	
	delay = time - end_time;
}

