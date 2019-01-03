// Sinclair Spectrum AY music emulator core

// Game_Music_Emu $vers
#ifndef AY_CORE_H
#define AY_CORE_H

#include "Z80_Cpu.h"
#include "Ay_Apu.h"

class Ay_Core {
public:
	
	// Clock count
	typedef int time_t;
	
	// Sound chip access, to assign it to Blip_Buffer etc.
	Ay_Apu& apu()                                   { return apu_; }
	
	// Sets beeper sound buffer, or NULL to mute it. Volume and treble EQ of
	// beeper are set by APU.
	void set_beeper_output( Blip_Buffer* );
	
	// Sets time between calls to play routine. Can be changed while playing.
	void set_play_period( time_t p )                { play_period = p; }
	
	// 64K memory to load code and data into before starting track. Caller
	// must parse the AY file.
	BOOST::uint8_t* mem()                           { return mem_.ram; }
	enum { mem_size = 0x10000 };
	enum { ram_addr = 0x4000 }; // where official RAM starts
	
	// Starts track using specified register values, and sets play routine that
	// is called periodically
	typedef Z80_Cpu::registers_t registers_t;
	typedef int addr_t;
	void start_track( registers_t const&, addr_t play );
	
	// Ends time frame of at most *end clocks and sets *end to number of clocks
	// emulated. Until Spectrum/CPC mode is determined, *end is HALVED.
	void end_frame( time_t* end );
	
	// Called when CPC hardware is first accessed. AY file format doesn't specify
	// which sound hardware is used, so it must be determined during playback
	// based on which sound port is first used.
	blargg_callback<void (*)( void* )> set_cpc_callback;

// Implementation
public:
	Ay_Core();
	~Ay_Core();

private:
	Blip_Buffer* beeper_output;
	int          beeper_delta;
	int          last_beeper;
	int          beeper_mask;
	
	addr_t play_addr;
	time_t play_period;
	time_t next_play;
	
	int  cpc_latch;
	bool spectrum_mode;
	bool cpc_mode;
	
	// large items
	Z80_Cpu cpu;
	struct {
		BOOST::uint8_t padding1 [0x100];
		BOOST::uint8_t ram      [mem_size + 0x100];
	} mem_;
	Ay_Apu apu_;
	
	int  cpu_in( addr_t );
	void cpu_out(  time_t, addr_t, int data );
	void cpu_out_( time_t, addr_t, int data );
	bool run_cpu( time_t end );
	void disable_beeper();
};

#endif
