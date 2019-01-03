// Atari XL/XE SAP core CPU and RAM emulator

// Game_Music_Emu $vers
#ifndef SAP_CORE_H
#define SAP_CORE_H

#include "Sap_Apu.h"
#include "Nes_Cpu.h"

class Sap_Core {
public:
	
	// Sound chips and common state
	Sap_Apu& apu()                              { return apu_; }
	Sap_Apu& apu2()                             { return apu2_; }
	Sap_Apu_Impl& apu_impl()                    { return apu_impl_; }
	
	// Adjusts music tempo, where 1.0 is normal. Can be changed while playing.
	void set_tempo( double );
	
	// Clears RAM and sets up default vectors, etc.
	void setup_ram();
	
	// 64K RAM to load file data blocks into
	BOOST::uint8_t* ram()                       { return mem.ram; }
	
	// Calls init routine and configures playback. RAM must have been
	// set up already.
	struct info_t {
		int init_addr;
		int play_addr;
		int music_addr;
		int type;
		int fastplay;
		bool stereo;
	};
	blargg_err_t start_track( int track, info_t const& );
	
	// Ends time frame at time t, then begins new at time 0
	typedef Nes_Cpu::time_t time_t; // Clock count
	blargg_err_t end_frame( time_t t );
	

// Implementation
public:
	Sap_Core();

private:
	enum { base_scanline_period = 114 };
	enum { lines_per_frame = 312 };
	typedef Nes_Cpu::addr_t addr_t;
	
	time_t scanline_period;
	time_t next_play;
	time_t time_mask;
	time_t frame_start;
	Nes_Cpu cpu;
	Nes_Cpu::registers_t saved_state;
	info_t info;
	Sap_Apu apu_;
	Sap_Apu apu2_;
	
	// large items
	struct {
		BOOST::uint8_t padding1 [  0x100];
		BOOST::uint8_t ram      [0x10000];
		BOOST::uint8_t padding2 [  0x100];
	} mem; // TODO: put on freestore
	Sap_Apu_Impl apu_impl_;

	void push( int b );
	void jsr_then_stop( addr_t );
	void run_routine( addr_t );
	void call_init( int track );
	bool run_cpu( time_t end );
	int  play_addr();
	int  read_d40b();
	int  read_mem( addr_t );
	void write_D2xx( int d2xx, int data );
	
	time_t time() const                     { return cpu.time() & time_mask; }
	blargg_err_t run_until( time_t t );
	time_t play_period() const { return info.fastplay * scanline_period; }
};

inline void Sap_Core::set_tempo( double t )
{
	scanline_period = (int) (base_scanline_period / t + 0.5);
}

#endif
