// Loads NSF file and emulates CPU and RAM, no sound chips

// Game_Music_Emu $vers
#ifndef NSF_IMPL_H
#define NSF_IMPL_H

#include "Gme_Loader.h"
#include "Nes_Cpu.h"
#include "Rom_Data.h"
#include "Nes_Apu.h"

// NSF file header
struct nsf_header_t
{
	typedef unsigned char byte;
	enum { size = 0x80 };
	
	char tag        [ 5];
	byte vers;
	byte track_count;
	byte first_track;
	byte load_addr  [ 2];
	byte init_addr  [ 2];
	byte play_addr  [ 2];
	char game       [32]; // NOT null-terminated if 32 chars in length
	char author     [32];
	char copyright  [32];
	byte ntsc_speed [ 2];
	byte banks      [ 8];
	byte pal_speed  [ 2];
	byte speed_flags;
	byte chip_flags;
	byte unused     [ 4];
	
	// Sound chip masks
	enum {
		vrc6_mask  = 1 << 0,
		vrc7_mask  = 1 << 1,
		fds_mask   = 1 << 2,
		mmc5_mask  = 1 << 3,
		namco_mask = 1 << 4,
		fme7_mask  = 1 << 5,
		all_mask   = (1 << 6) - 1
	};
	
	// True if header has proper NSF file signature
	bool valid_tag() const;
	
	// True if file supports only PAL speed
	bool pal_only() const           { return (speed_flags & 3) == 1; }
	
	// Clocks per second
	double clock_rate() const;
	
	// Clocks between calls to play routine
	int play_period() const;
};

/* Loads NSF file into memory, then emulates CPU, RAM, and ROM.
Non-memory accesses are routed through cpu_read() and cpu_write(). */
class Nsf_Impl : public Gme_Loader {
public:

	// Sound chip
	Nes_Apu* nes_apu()                  { return &apu; }
	
	// Starts track, where 0 is the first
	virtual blargg_err_t start_track( int );
	
	// Emulates to at least time t, then begins new time frame at
	// time t. Might emulate a few clocks extra, so after returning,
	// time() may not be zero.
	typedef int time_t; // clock count
	virtual void end_frame( time_t n );
	
// Finer control

	// Header for currently loaded file
	typedef nsf_header_t header_t;
	header_t const& header() const      { return header_; }
	
	// Sets clocks between calls to play routine to p + 1/2 clock
	void set_play_period( int p )       { play_period = p; }
	
	// Time play routine will next be called
	time_t play_time() const        { return next_play; }
	
	// Emulates to at least time t. Might emulate a few clocks extra.
	virtual void run_until( time_t t );
	
	// Time emulated to
	time_t time() const             { return cpu.time(); }

	void enable_w4011_(bool enable = true) { enable_w4011 = enable; }

	Rom_Data const& rom_() const { return rom; }
	
protected:
// Nsf_Core use

	typedef int addr_t;
	
	// Called for unmapped accesses. Default just prints info if debugging.
	virtual void unmapped_write( addr_t, int data );
	virtual int  unmapped_read(  addr_t );
	
	// Override in derived class
	// Bank writes and RAM at 0-$7FF and $6000-$7FFF are handled internally
	virtual int  cpu_read(  addr_t a )          { return unmapped_read( a ); }
	virtual void cpu_write( addr_t a, int data ){ unmapped_write( a, data ); }
	
	// Reads byte as CPU would when executing code. Only works for RAM/ROM,
	// NOT I/O like sound chips.
	int  read_code( addr_t addr ) const;

// Debugger services

	enum { mem_size  = 0x10000 };
	
	// CPU sits here when waiting for next call to play routine
	enum { idle_addr = 0x5FF6 };
	
	Nes_Cpu cpu;
	
	// Runs CPU to at least time t and returns false, or returns true
	// if it encounters illegal instruction (halt).
	virtual bool run_cpu_until( time_t t );
	
	// CPU calls through to these to access memory (except instructions)
	int  read_mem(  addr_t );
	void write_mem( addr_t, int );
	
	// Address of play routine
	addr_t play_addr() const        { return get_addr( header_.play_addr ); }
	
	// Same as run_until, except emulation stops for any event (routine returned,
	// play routine called, illegal instruction).
	void run_once( time_t );
	
	// Make a note of event
	virtual void special_event( const char str [] );
	

// Implementation
public:
	Nsf_Impl();
	~Nsf_Impl();

protected:
	virtual blargg_err_t load_( Data_Reader& );
	virtual void unload();

private:
	enum { low_ram_size = 0x800 };
	enum { fdsram_size  = 0x6000 };
	enum { sram_size    = 0x2000 };
	enum { unmapped_size= Nes_Cpu::page_size + 8 };
	enum { fds_banks    = 2 };
	enum { bank_count   = fds_banks + 8 };
	enum { banks_addr   = idle_addr };
	enum { sram_addr    = 0x6000 };
	
	blargg_vector<byte> high_ram;
	Rom_Data rom;
	
	// Play routine timing
	time_t next_play;
	time_t play_period;
	int play_extra;
	int play_delay;
	bool enable_w4011;
	Nes_Cpu::registers_t saved_state; // of interrupted init routine
	
	// Large objects after others
	header_t header_;
	Nes_Apu apu;
	byte low_ram [low_ram_size];
	
	// Larger RAM areas allocated separately
	enum { fdsram_offset = sram_size + unmapped_size };
	byte* sram()            { return high_ram.begin(); }
	byte* unmapped_code()   { return &high_ram [sram_size]; }
	byte* fdsram()          { return &high_ram [fdsram_offset]; }
	int fds_enabled() const { return header_.chip_flags & header_t::fds_mask; }
	
	void map_memory();
	void write_bank( int index, int data );
	void jsr_then_stop( byte const addr [] );
	void push_byte( int );
	static addr_t get_addr( byte const [] );
	static int pcm_read( void*, int );
};

#endif
