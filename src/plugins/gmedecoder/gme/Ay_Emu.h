// Sinclair Spectrum AY music file emulator

// Game_Music_Emu $vers
#ifndef AY_EMU_H
#define AY_EMU_H

#include "Classic_Emu.h"
#include "Ay_Core.h"

class Ay_Emu : public Classic_Emu {
public:
	// AY file header
	struct header_t
	{
		enum { size = 0x14 };
		
		byte tag        [8];
		byte vers;
		byte player;
		byte unused     [2];
		byte author     [2];
		byte comment    [2];
		byte max_track;
		byte first_track;
		byte track_info [2];
	};
	
	static gme_type_t static_type() { return gme_ay_type; }

// Implementation
public:
	Ay_Emu();
	~Ay_Emu();

	struct file_t {
		header_t const* header;
		byte const* tracks;
		byte const* end;    // end of file data
	};

	blargg_err_t hash_( Hash_Function& out ) const;
	
protected:
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_mem_( byte const [], int );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );

private:
	file_t file;
	Ay_Core core;
	
	void enable_cpc();
	static void enable_cpc_( void* data );
};

#endif
