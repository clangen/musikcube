// Nintendo Game Boy GBS music file emulator core

// Game_Music_Emu $vers
#ifndef GBS_CORE_H
#define GBS_CORE_H

#include "Gme_Loader.h"
#include "Rom_Data.h"
#include "Gb_Cpu.h"
#include "Gb_Apu.h"

class Gbs_Core : public Gme_Loader {
public:

	// GBS file header
	struct header_t
	{
		enum { size = 112 };
		
		char tag       [ 3];
		byte vers;
		byte track_count;
		byte first_track;
		byte load_addr [ 2];
		byte init_addr [ 2];
		byte play_addr [ 2];
		byte stack_ptr [ 2];
		byte timer_modulo;
		byte timer_mode;
		char game      [32]; // strings can be 32 chars, NOT terminated
		char author    [32];
		char copyright [32];
		
		// True if header has valid file signature
		bool valid_tag() const;
	};
	
	// Header for currently loaded file
	header_t const& header() const      { return header_; }
	
	// Sound chip
	Gb_Apu& apu()                       { return apu_; }

	// ROM data
	Rom_Data const& rom_() const         { return rom; }
	
	// Adjusts music tempo, where 1.0 is normal. Can be changed while playing.
	void set_tempo( double );
	
	// Starts track, where 0 is the first. Uses specified APU mode.
	blargg_err_t start_track( int, Gb_Apu::mode_t = Gb_Apu::mode_cgb );
	
	// Ends time frame at time t
	typedef int time_t; // clock count
	blargg_err_t end_frame( time_t t );
	
	// Clocks between calls to play routine
	time_t play_period() const          { return play_period_; }
	
protected:
	typedef int addr_t;
	
	// Current time
	time_t time() const                 { return cpu.time() + end_time; }
	
	// Runs emulator to time t
	blargg_err_t run_until( time_t t );

	// Runs CPU until time becomes >= 0
	void run_cpu();
	
	// Reads/writes memory and I/O
	int  read_mem(  addr_t );
	void write_mem( addr_t, int );

// Implementation
public:
	Gbs_Core();
	~Gbs_Core();
	virtual void unload();

protected:
	virtual blargg_err_t load_( Data_Reader& );
	
private:
	enum { ram_addr = 0xA000 };
	enum { io_base = 0xFF00 };
	enum { hi_page = io_base - ram_addr };
	
	Rom_Data  rom;
	int       tempo;
	time_t    end_time;
	time_t    play_period_;
	time_t    next_play;
	header_t  header_;
	Gb_Cpu    cpu;
	Gb_Apu    apu_;
	byte      ram [0x4000 + 0x2000 + Gb_Cpu::cpu_padding];
	
	void update_timer();
	void jsr_then_stop( byte const [] );
	void set_bank( int n );
	void write_io_inline( int offset, int data, int base );
	void write_io_( int offset, int data );
	int  read_io(  int offset );
	void write_io( int offset, int data );
};

#endif
