// Loads NSF file and emulates CPU and sound chips

// Game_Music_Emu $vers
#ifndef NSF_CORE_H
#define NSF_CORE_H

#include "Nsf_Impl.h"

class Nes_Namco_Apu;
class Nes_Vrc6_Apu;
class Nes_Fme7_Apu;
class Nes_Mmc5_Apu;
class Nes_Vrc7_Apu;
class Nes_Fds_Apu;

class Nsf_Core : public Nsf_Impl {
public:
	
	// Adjusts music tempo, where 1.0 is normal. Can be changed while playing.
	// Loading a file resets tempo to 1.0.
	void set_tempo( double );
	
	// Pointer to sound chip, or NULL if not used by current file.
	// Must be assigned to a Blip_Buffer to get any sound.
	Nes_Fds_Apu  * fds_apu  () { return fds;   }
	Nes_Fme7_Apu * fme7_apu () { return fme7;  }
	Nes_Mmc5_Apu * mmc5_apu () { return mmc5;  }
	Nes_Namco_Apu* namco_apu() { return namco; }
	Nes_Vrc6_Apu * vrc6_apu () { return vrc6;  }
	Nes_Vrc7_Apu * vrc7_apu () { return vrc7;  }
	
	// Mask for which chips are supported
	#if NSF_EMU_APU_ONLY
		enum { chips_mask = 0 };
	#else
		enum { chips_mask = header_t::all_mask };
	#endif

protected:
	virtual int  unmapped_read(  addr_t );
	virtual void unmapped_write( addr_t, int data );


// Implementation
public:
	Nsf_Core();
	~Nsf_Core();
	virtual void unload();
	virtual blargg_err_t start_track( int );
	virtual void end_frame( time_t );

protected:
	virtual blargg_err_t post_load();
	virtual int  cpu_read(  addr_t );
	virtual void cpu_write( addr_t, int );

private:
	byte mmc5_mul [2];
	
	Nes_Fds_Apu*   fds;
	Nes_Fme7_Apu*  fme7;
	Nes_Mmc5_Apu*  mmc5;
	Nes_Namco_Apu* namco;
	Nes_Vrc6_Apu*  vrc6;
	Nes_Vrc7_Apu*  vrc7;
};

#endif
