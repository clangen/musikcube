// Fir_Resampler chip emulator container that mixes into the output buffer

// Game_Music_Emu $vers
#ifndef CHIP_RESAMPLER_H
#define CHIP_RESAMPLER_H

#include "blargg_source.h"

#include "Fir_Resampler.h"
typedef Fir_Resampler_Norm Chip_Resampler_Downsampler;

int const resampler_extra = 0; //34;

template<class Emu>
class Chip_Resampler_Emu : public Emu {
	int last_time;
	short* out;
	typedef short dsample_t;
	enum { disabled_time = -1 };
	enum { gain_bits = 14 };
	blargg_vector<dsample_t> sample_buf;
	int sample_buf_size;
	int oversamples_per_frame;
	int buf_pos;
	int buffered;
	int resampler_size;
	int gain_;

	Chip_Resampler_Downsampler resampler;

	void mix_samples( short * buf, int count )
	{
        dsample_t * inptr = sample_buf.begin();
		for ( unsigned i = 0; i < count * 2; i++ )
		{
			int sample = inptr[i];
			sample += buf[i];
			if ((short)sample != sample) sample = 0x7FFF ^ (sample >> 31);
			buf[i] = sample;
		}
	}

public:
	Chip_Resampler_Emu()      { last_time = disabled_time; out = NULL; }
	blargg_err_t setup( double oversample, double rolloff, double gain )
	{
		gain_ = (int) ((1 << gain_bits) * gain);
		RETURN_ERR( resampler.set_rate( oversample ) );
		return reset_resampler();
	}

	blargg_err_t reset()
	{
		Emu::reset();
		resampler.clear();
		return blargg_ok;
	}

	blargg_err_t reset_resampler()
	{
		unsigned int pairs;
		double rate = resampler.rate();
		if ( rate >= 1.0 ) pairs = 64.0 * rate;
		else pairs = 64.0 / rate;
		RETURN_ERR( sample_buf.resize( (pairs + (pairs >> 2)) * 2 ) );
		resize( pairs );
		resampler_size = oversamples_per_frame + (oversamples_per_frame >> 2);
		RETURN_ERR( resampler.resize_buffer( resampler_size ) );
		return blargg_ok;
	}

	void resize( int pairs )
	{
		int new_sample_buf_size = pairs * 2;
		//new_sample_buf_size = new_sample_buf_size / 4 * 4; // TODO: needed only for 3:2 downsampler
		if ( sample_buf_size != new_sample_buf_size )
		{
			if ( (unsigned) new_sample_buf_size > sample_buf.size() )
			{
				check( false );
				return;
			}
			sample_buf_size = new_sample_buf_size;
			oversamples_per_frame = int (pairs * resampler.rate()) * 2 + 2;
			clear();
		}
	}

	void clear()
	{
		buf_pos = buffered = 0;
		resampler.clear();
	}

	void enable( bool b = true )    { last_time = b ? 0 : disabled_time; }
	bool enabled() const            { return last_time != disabled_time; }
	void begin_frame( short* buf )  { out = buf; last_time = 0; }

	int run_until( int time )
	{
		int count = time - last_time;
		while ( count > 0 )
		{
			if ( last_time < 0 )
				return false;
			last_time = time;
			if ( buffered )
			{
				int samples_to_copy = buffered;
				if ( samples_to_copy > count ) samples_to_copy = count;
				memcpy( out, sample_buf.begin(), samples_to_copy * sizeof(short) * 2 );
				memcpy( sample_buf.begin(), sample_buf.begin() + samples_to_copy * 2, ( buffered - samples_to_copy ) * 2 * sizeof(short) );
				buffered -= samples_to_copy;
				count -= samples_to_copy;
				continue;
			}
			int sample_count = oversamples_per_frame - resampler.written() + resampler_extra;
			memset( resampler.buffer(), 0, sample_count * sizeof(*resampler.buffer()) );
			Emu::run( sample_count >> 1, resampler.buffer() );
			for ( unsigned i = 0; i < sample_count; i++ )
			{
                dsample_t * ptr = resampler.buffer() + i;
				*ptr = ( *ptr * gain_ ) >> gain_bits;
			}
			short* p = out;
			resampler.write( sample_count );
			sample_count = resampler.read( sample_buf.begin(), count * 2 > sample_buf_size ? sample_buf_size : count * 2 ) >> 1;
			if ( sample_count > count )
			{
				out += count * Emu::out_chan_count;
				mix_samples( p, count );
				memmove( sample_buf.begin(), sample_buf.begin() + count * 2, (sample_count - count) * 2 * sizeof(short) );
				buffered = sample_count - count;
				return true;
			}
			else if (!sample_count) return true;
			out += sample_count * Emu::out_chan_count;
			mix_samples( p, sample_count );
			count -= sample_count;
		}
		return true;
	}
};



#endif
