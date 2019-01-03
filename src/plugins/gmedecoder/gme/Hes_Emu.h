// TurboGrafx-16/PC Engine HES music file emulator

// Game_Music_Emu $vers
#ifndef HES_EMU_H
#define HES_EMU_H

#include "Classic_Emu.h"
#include "Hes_Core.h"

class Hes_Emu : public Classic_Emu {
public:

	static gme_type_t static_type() { return gme_hes_type; }
	
	// HES file header (see Hes_Core.h)
	typedef Hes_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const { return core.header(); }

	blargg_err_t hash_( Hash_Function& ) const;

// Implementation
public:
	Hes_Emu();
	~Hes_Emu();
	virtual void unload();

protected:
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_( Data_Reader& );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );

private:
	Hes_Core core;
};

#endif
