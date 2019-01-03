// TurboGrafx-16/PC Engine HES music file emulator core

// Game_Music_Emu $vers
#ifndef HES_CORE_H
#define HES_CORE_H

#include "Gme_Loader.h"
#include "Rom_Data.h"
#include "Hes_Apu.h"
#include "Hes_Apu_Adpcm.h"
#include "Hes_Cpu.h"

class Hes_Core : public Gme_Loader {
public:

	// HES file header
	enum { info_offset = 0x20 };
	struct header_t
	{
		enum { size = 0x20 };
		
		byte tag       [4];
		byte vers;
		byte first_track;
		byte init_addr [2];
		byte banks     [8];
		byte data_tag  [4];
		byte data_size [4];
		byte addr      [4];
		byte unused    [4];
		
		// True if header has valid file signature
		bool valid_tag() const;
	};
	
	// Header for currently loaded file
	header_t const& header() const      { return header_; }
	
	// Pointer to ROM data, for getting track information from
	byte const* data() const            { return rom.begin(); }
	int data_size() const               { return rom.file_size(); }
	
	// Adjusts rate play routine is called at, where 1.0 is normal.
	// Can be changed while track is playing.
	void set_tempo( double );
	
	// Sound chip
	Hes_Apu& apu()                      { return apu_; }

	Hes_Apu_Adpcm& adpcm()              { return adpcm_; }
	
	// Starts track
	blargg_err_t start_track( int );
	
	// Ends time frame at time t
	typedef int time_t;
	blargg_err_t end_frame( time_t );

// Implementation
public:
	Hes_Core();
	~Hes_Core();
	virtual void unload();
	
protected:
	virtual blargg_err_t load_( Data_Reader& );

private:
	enum { idle_addr = 0x1FFF };
	
	typedef int addr_t;
	Hes_Cpu  cpu;
	Rom_Data rom;
	header_t header_;
	time_t   play_period;
	int      timer_base;
	
	struct {
		time_t last_time;
		int    count;
		int    load;
		int    raw_load;
		byte   enabled;
		byte   fired;
	} timer;
	
	struct {
		time_t next_vbl;
		byte   latch;
		byte   control;
	} vdp;
	
	struct {
		time_t timer;
		time_t vdp;
		byte   disables;
	} irq;
	
	void recalc_timer_load();
	
	// large items
	byte*   write_pages [Hes_Cpu::page_count + 1]; // 0 if unmapped or I/O space
	Hes_Apu apu_;
	Hes_Apu_Adpcm adpcm_;
	byte    ram [Hes_Cpu::page_size];
	byte    sgx [3 * Hes_Cpu::page_size + Hes_Cpu::cpu_padding];
	
	void irq_changed();
	void run_until( time_t );
	bool run_cpu( time_t end );
	int  read_mem_( addr_t );
	int  read_mem( addr_t );
	void write_mem_( addr_t, int data );
	void write_mem( addr_t, int );
	void write_vdp( int addr, int data );
	void set_mmr( int reg, int bank );
	int  cpu_done();
};

#endif
