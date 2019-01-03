// MSX computer KSS music file emulator

// Game_Music_Emu $vers
#ifndef KSS_CORE_H
#define KSS_CORE_H

#include "Gme_Loader.h"
#include "Rom_Data.h"
#include "Z80_Cpu.h"

class Kss_Core : public Gme_Loader {
public:
	// KSS file header
	struct header_t
	{
		enum { size = 0x20 };
		enum { base_size = 0x10 };
		enum { ext_size = size - base_size };
		
		byte tag [4];
		byte load_addr [2];
		byte load_size [2];
		byte init_addr [2];
		byte play_addr [2];
		byte first_bank;
		byte bank_mode;
		byte extra_header;
		byte device_flags;
		
		// KSSX extended data, if extra_header==0x10
		byte data_size [4];
		byte unused [4];
		byte first_track [2];
		byte last_track [2]; // if no extended data, we set this to 0xFF
		byte psg_vol;
		byte scc_vol;
		byte msx_music_vol;
		byte msx_audio_vol;
	};
	
	// Header for currently loaded file
	header_t const& header() const { return header_; }

	// ROM data
	Rom_Data const& rom_() const { return rom; }
	
	typedef int time_t;
	void set_play_period( time_t p )        { play_period = p; }
	
	blargg_err_t start_track( int );
	
	blargg_err_t end_frame( time_t );

protected:
	typedef Z80_Cpu Kss_Cpu;
	Kss_Cpu cpu;
	
	void set_bank( int logical, int physical );
	
	typedef int addr_t;
	virtual void cpu_write( addr_t, int ) = 0;
	virtual int  cpu_in(  time_t, addr_t );
	virtual void cpu_out( time_t, addr_t, int );
	
	// Called after one frame of emulation
	virtual void update_gain() = 0;
	
// Implementation
public:
	Kss_Core();
	virtual ~Kss_Core();

protected:
	virtual blargg_err_t load_( Data_Reader& );
	virtual void unload();

private:
	enum { idle_addr = 0xFFFF };
	
	Rom_Data rom;
	header_t header_;
	bool gain_updated;
	int bank_count;
	time_t play_period;
	time_t next_play;
	
	// large items
	enum { mem_size = 0x10000 };
	byte ram [mem_size + Kss_Cpu::cpu_padding];
	byte unmapped_read  [0x100]; // TODO: why isn't this page_size?
	// because CPU can't read beyond this in last page? or because it will spill into unmapped_write?
	
	byte unmapped_write [Kss_Cpu::page_size];
	
	int bank_size() const { return (16 * 1024) >> (header_.bank_mode >> 7 & 1); }
	bool run_cpu( time_t end );
	void jsr( byte const (&addr) [2] );
};

#endif
