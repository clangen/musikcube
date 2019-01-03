// Game_Music_Emu $vers. http://www.slack.net/~ant/

#include "Qsound_Apu.h"
#include "qmix.h"

Qsound_Apu::Qsound_Apu() { chip = 0; rom = 0; rom_size = 0; sample_rate = 0; }

Qsound_Apu::~Qsound_Apu()
{
    if ( chip ) free( chip );
    if ( rom ) free( rom );
}

int Qsound_Apu::set_rate( int clock_rate )
{
    if ( chip )
	{
        free( chip );
		chip = 0;
	}
	
    chip = malloc( _qmix_get_state_size() );
	if ( !chip )
		return 0;
	
	reset();

    return clock_rate / 166;
}

void Qsound_Apu::set_sample_rate( int sample_rate )
{
	this->sample_rate = sample_rate;
	if ( chip ) _qmix_set_sample_rate( chip, sample_rate );
}

void Qsound_Apu::reset()
{
    _qmix_clear_state( chip );
	_qmix_set_sample_rate( chip, sample_rate );
    if ( rom ) _qmix_set_sample_rom( chip, rom, rom_size );
}

void Qsound_Apu::write( int addr, int data )
{
    _qmix_command( chip, addr, data );
}

void Qsound_Apu::write_rom( int size, int start, int length, void const* data )
{
    if ( size > rom_size )
    {
        rom_size = size;
        rom = realloc( rom, size );
    }
    if ( start > size ) start = size;
    if ( start + length > size ) length = size - start;
    memcpy( (uint8*)rom + start, data, length );
    if ( chip ) _qmix_set_sample_rom( chip, rom, rom_size );
}

void Qsound_Apu::run( int pair_count, sample_t* out )
{
    sint16 buf[ 1024 * 2 ];

	while (pair_count > 0)
	{
		int todo = pair_count;
		if (todo > 1024) todo = 1024;
        _qmix_render( chip, buf, todo );

        for (int i = 0; i < todo * 2; i++)
		{
            int output = buf [i];
            output += out [0];
            if ( (short)output != output ) output = 0x7FFF ^ ( output >> 31 );
            out [0] = output;
            out++;
		}

		pair_count -= todo;
	}
}
