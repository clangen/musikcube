#include "Opl_Apu.h"

#include "blargg_source.h"

#include "ym2413.h"
#include "fmopl.h"

Opl_Apu::Opl_Apu() { opl = 0; opl_memory = 0; }

blargg_err_t Opl_Apu::init( long clock, long rate, blip_time_t period, type_t type )
{
	type_ = type;
	clock_ = clock;
	rate_ = rate;
	period_ = period;
	set_output( 0, 0 );
	volume( 1.0 );
	switch (type)
	{
	case type_opll:
	case type_msxmusic:
	case type_smsfmunit:
		opl = ym2413_init( clock, rate, 0 );
		break;

	case type_vrc7:
		opl = ym2413_init( clock, rate, 1 );
		break;

	case type_opl:
		opl = ym3526_init( clock, rate );
		break;

	case type_msxaudio:
		//logfile = fopen("c:\\temp\\msxaudio.log", "wb");
		opl = y8950_init( clock, rate );
		opl_memory = malloc( 32768 );
		y8950_set_delta_t_memory( opl, opl_memory, 32768 );
		break;

	case type_opl2:
		opl = ym3812_init( clock, rate );
		break;
	}
	reset();
	return 0;
}

Opl_Apu::~Opl_Apu()
{
	if (opl)
	{
		switch (type_)
		{
		case type_opll:
		case type_msxmusic:
		case type_smsfmunit:
		case type_vrc7:
			ym2413_shutdown( opl );
			break;

		case type_opl:
			ym3526_shutdown( opl );
			break;

		case type_msxaudio:
			y8950_shutdown( opl );
			free( opl_memory );
			//fclose( logfile );
			break;

		case type_opl2:
			ym3812_shutdown( opl );
			break;
		}
	}
}

void Opl_Apu::reset()
{
	addr = 0;
	next_time = 0;
	last_amp = 0;

	switch (type_)
	{
	case type_opll:
	case type_msxmusic:
	case type_smsfmunit:
	case type_vrc7:
		ym2413_reset_chip( opl );
		break;

	case type_opl:
		ym3526_reset_chip( opl );
		break;

	case type_msxaudio:
		y8950_reset_chip( opl );
		break;

	case type_opl2:
		ym3812_reset_chip( opl );
		break;
	}
}

void Opl_Apu::write_data( blip_time_t time, int data )
{
	run_until( time );
	switch (type_)
	{
	case type_opll:
	case type_msxmusic:
	case type_smsfmunit:
	case type_vrc7:
		ym2413_write( opl, 0, addr );
		ym2413_write( opl, 1, data );
		break;

	case type_opl:
		ym3526_write( opl, 0, addr );
		ym3526_write( opl, 1, data );
		break;

	case type_msxaudio:
		/*if ( addr >= 7 && addr <= 7 + 11 )
		{
			unsigned char temp [2] = { addr - 7, data };
			fwrite( &temp, 1, 2, logfile );
		}*/
		y8950_write( opl, 0, addr );
		y8950_write( opl, 1, data );
		break;

	case type_opl2:
		ym3812_write( opl, 0, addr );
		ym3812_write( opl, 1, data );
		break;
	}
}

int Opl_Apu::read( blip_time_t time, int port )
{
	run_until( time );
	switch (type_)
	{
	case type_opll:
	case type_msxmusic:
	case type_smsfmunit:
	case type_vrc7:
		return ym2413_read( opl, port );

	case type_opl:
		return ym3526_read( opl, port );

	case type_msxaudio:
		{
			int ret = y8950_read( opl, port );
			/*unsigned char temp [2] = { port + 0x80, ret };
			fwrite( &temp, 1, 2, logfile );*/
			return ret;
		}

	case type_opl2:
		return ym3812_read( opl, port );
	}

	return 0;
}

void Opl_Apu::end_frame( blip_time_t time )
{
	run_until( time );
	next_time -= time;

	if ( output_ )
		output_->set_modified();
}

void Opl_Apu::run_until( blip_time_t end_time )
{
	if ( end_time > next_time )
	{
		blip_time_t time_delta = end_time - next_time;
		blip_time_t time = next_time;
		unsigned count = time_delta / period_ + 1;
		switch (type_)
		{
		case type_opll:
		case type_msxmusic:
		case type_smsfmunit:
		case type_vrc7:
			{
				SAMP bufMO[ 1024 ];
				SAMP bufRO[ 1024 ];
				SAMP * buffers[2] = { bufMO, bufRO };

				while ( count > 0 )
				{
					unsigned todo = count;
					if ( todo > 1024 ) todo = 1024;
					ym2413_update_one( opl, buffers, todo );

					if ( output_ )
					{
						int last_amp = this->last_amp;
						for ( unsigned i = 0; i < todo; i++ )
						{
							int amp = bufMO [i] + bufRO [i];
							int delta = amp - last_amp;
							if ( delta )
							{
								last_amp = amp;
								synth.offset_inline( time, delta, output_ );
							}
							time += period_;
						}
						this->last_amp = last_amp;
					}
					else time += period_ * todo;

					count -= todo;
				}
			}
			break;

		case type_opl:
		case type_msxaudio:
		case type_opl2:
			{
				OPLSAMPLE buffer[ 1024 ];

				while ( count > 0 )
				{
					unsigned todo = count;
					if ( todo > 1024 ) todo = 1024;
					switch (type_)
					{
					case type_opl:      ym3526_update_one( opl, buffer, todo ); break;
					case type_msxaudio: y8950_update_one( opl, buffer, todo ); break;
					case type_opl2:     ym3812_update_one( opl, buffer, todo ); break;
					default: break;
					}

					if ( output_ )
					{
						int last_amp = this->last_amp;
						for ( unsigned i = 0; i < todo; i++ )
						{
							int amp = buffer [i];
							int delta = amp - last_amp;
							if ( delta )
							{
								last_amp = amp;
								synth.offset_inline( time, delta, output_ );
							}
							time += period_;
						}
						this->last_amp = last_amp;
					}
					else time += period_ * todo;

					count -= todo;
				}
			}
			break;
		}
		next_time = time;
	}
}