// Sega/Game Gear/Coleco SGC music file emulator

// Game_Music_Emu $vers
#ifndef SGC_EMU_H
#define SGC_EMU_H

#include "Classic_Emu.h"
#include "Sgc_Core.h"

class Sgc_Emu : public Classic_Emu {
public:
	// SGC file header (see Sgc_Impl.h)
	typedef Sgc_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const              { return core_.header(); }

	blargg_err_t hash_( Hash_Function& ) const;
	
	// Sets 0x2000-byte Coleco BIOS. Necessary to play Coleco tracks.
	static void set_coleco_bios( void const* p ){ Sgc_Core::set_coleco_bios( p ); }
	
	static gme_type_t static_type()             { return gme_sgc_type; }
	
// Internal
public:
	Sgc_Emu();
	~Sgc_Emu();

protected:
	// Classic_Emu overrides
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_( Data_Reader& );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );
	virtual void unload();
	
private:
	Sgc_Core core_;
};

#endif
