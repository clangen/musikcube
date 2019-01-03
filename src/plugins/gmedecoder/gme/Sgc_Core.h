// Sega/Game Gear/Coleco SGC music file emulator core

// Game_Music_Emu $vers
#ifndef SGC_CORE_H
#define SGC_CORE_H

#include "Sgc_Impl.h"
#include "Sms_Fm_Apu.h"
#include "Sms_Apu.h"

class Sgc_Core : public Sgc_Impl {
public:
	
	// Adjusts music tempo, where 1.0 is normal. Can be changed while playing.
	// Resets to 1.0 when loading file.
	void set_tempo( double );
	
	// Starts track, where 0 is the first.
	blargg_err_t start_track( int );
	
	// Ends time frame at time t
	blargg_err_t end_frame( time_t t );
	
	// SN76489 sound chip
	Sms_Apu& apu()                  { return apu_; }
	Sms_Fm_Apu& fm_apu()            { return fm_apu_; }
	
protected:
	// Overrides
	virtual void cpu_out( time_t, addr_t, int data );
	virtual blargg_err_t load_( Data_Reader& );

// Implementation
public:
	Sgc_Core();
	~Sgc_Core();

private:
	bool fm_accessed;
	Sms_Apu apu_;
	Sms_Fm_Apu fm_apu_;
};

#endif
