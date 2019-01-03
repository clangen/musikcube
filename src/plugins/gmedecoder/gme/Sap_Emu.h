// Atari XL/XE SAP music file emulator

// Game_Music_Emu $vers
#ifndef SAP_EMU_H
#define SAP_EMU_H

#include "Classic_Emu.h"
#include "Sap_Apu.h"
#include "Sap_Core.h"

class Sap_Emu : public Classic_Emu {
public:
	enum { max_tracks = 32 }; // TODO: no fixed limit
	
	// SAP file info (see Sap_Core.h for more)
	struct info_t : Sap_Core::info_t {
		byte const* rom_data;
		const char* warning;
		int  track_count;
		int  track_times [max_tracks];
		char author    [256];
		char name      [256];
		char copyright [ 32];
	};
	
	// Info for currently loaded file
	info_t const& info() const { return info_; }

	blargg_err_t hash_( Hash_Function& ) const;

	static gme_type_t static_type() { return gme_sap_type; }

// Implementation
public:
	Sap_Emu();
	~Sap_Emu();

protected:
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_mem_( byte const [], int );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );

private:
	info_t info_;
	byte const* file_end;
	Sap_Core core;
};

#endif
