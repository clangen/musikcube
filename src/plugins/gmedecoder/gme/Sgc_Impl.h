// Sega/Game Gear/Coleco SGC music file emulator implementation internals

// Game_Music_Emu $vers
#ifndef SGC_IMPL_H
#define SGC_IMPL_H

#include "Gme_Loader.h"
#include "Rom_Data.h"
#include "Z80_Cpu.h"

class Sgc_Impl : public Gme_Loader {
public:
	
	// SGC file header
	struct header_t
	{
		enum { size = 0xA0 };
		
		char tag       [4]; // "SGC\x1A"
		byte vers;          // 0x01
		byte rate;          // 0=NTSC 1=PAL
		byte reserved1 [2];
		byte load_addr [2];
		byte init_addr [2];
		byte play_addr [2];
		byte stack_ptr [2];
		byte reserved2 [2];
		byte rst_addrs [7*2];
		byte mapping   [4]; // Used by Sega only
		byte first_song;    // Song to start playing first
		byte song_count;
		byte first_effect;
		byte last_effect;
		byte system;        // 0=Master System 1=Game Gear 2=Colecovision
		byte reserved3 [23];
		char game      [32]; // strings can be 32 chars, NOT terminated
		char author    [32];
		char copyright [32];
		
		// True if header has valid file signature
		bool valid_tag() const;
		
		int effect_count() const        { return last_effect ? last_effect - first_effect + 1 : 0; }
	};
	
	// Header for currently loaded file
	header_t const& header() const                  { return header_; }

	Rom_Data const& rom_() const                    { return rom; }
	
	int clock_rate() const                          { return header_.rate ? 3546893 : 3579545; }
	
	// 0x2000 bytes
	static void set_coleco_bios( void const* p )    { coleco_bios = p; }
	
	// Clocks between calls to play routine
	typedef int time_t;
	void set_play_period( time_t p )                { play_period = p; }
	
	// 0 = first track
	blargg_err_t start_track( int );
	
	// Runs for t clocks
	blargg_err_t end_frame( time_t t );
	
	// True if Master System or Game Gear
	bool sega_mapping() const;
	
protected:
	typedef Z80_Cpu Sgc_Cpu;
	Sgc_Cpu cpu;
	
	typedef int addr_t;
	virtual void cpu_out( time_t, addr_t, int data ) BLARGG_PURE( ; )

// Implementation
public:
	Sgc_Impl();
	~Sgc_Impl();
	virtual void unload();

protected:
	virtual blargg_err_t load_( Data_Reader& );

private:
	enum { bank_size = 0x4000 };
	
	Rom_Data rom;
	time_t play_period;
	time_t next_play;
	void const* bank2;      // ROM selected for bank 2, in case RAM is currently hiding it
	addr_t vectors_addr;    // RST vectors start here
	addr_t idle_addr;       // return address for init/play routines
	static void const* coleco_bios;
	
	// large items
	header_t header_;
	blargg_vector<byte> vectors;
	blargg_vector<byte> ram;
	blargg_vector<byte> ram2;
	blargg_vector<byte> unmapped_write;
	
	bool run_cpu( time_t end );
	void jsr( byte const (&addr) [2] );
	void cpu_write( addr_t, int data );
	int  cpu_in(  addr_t );
	
	void set_bank( int bank, void const* data );
};

inline bool Sgc_Impl::sega_mapping() const
{
	return header_.system <= 1;
}

#endif
