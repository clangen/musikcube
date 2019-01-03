// MSX computer KSS music file emulator

// Game_Music_Emu $vers
#ifndef KSS_EMU_H
#define KSS_EMU_H

#include "Classic_Emu.h"
#include "Kss_Core.h"
#include "Kss_Scc_Apu.h"
#include "Sms_Apu.h"
#include "Ay_Apu.h"
#include "Opl_Apu.h"

class Kss_Emu : public Classic_Emu {
public:
	// KSS file header (see Kss_Core.h)
	typedef Kss_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const { return core.header(); }

	blargg_err_t hash_( Hash_Function& ) const;
	
	static gme_type_t static_type() { return gme_kss_type; }

// Implementation
public:
	Kss_Emu();
	~Kss_Emu();

protected:
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t load_( Data_Reader& );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t run_clocks( blip_time_t&, int );
	virtual void set_tempo_( double );
	virtual void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	virtual void update_eq( blip_eq_t const& );
	virtual void unload();
	
private:
	struct Core;
	friend struct Core;
	struct Core : Kss_Core {
		Kss_Emu& emu;
		
		// detection of tunes that use SCC so they can be made louder
		bool scc_accessed;
		
		enum { scc_enabled_true = 0xC000 };
		unsigned scc_enabled; // 0 or 0xC000
		int ay_latch;
		
		struct {
			Sms_Apu* psg;
			Opl_Apu* fm;
		} sms;
		
		struct {
			Ay_Apu*  psg;
			Scc_Apu* scc;
			Opl_Apu* music;
			Opl_Apu* audio;
		} msx;
		
		Core( Kss_Emu* e ) : emu( *e ) { }
		
		virtual void cpu_write( addr_t, int );
		virtual int  cpu_in(  time_t, addr_t );
		virtual void cpu_out( time_t, addr_t, int );
		virtual void update_gain();

		void cpu_write_( addr_t addr, int data );
		void update_gain_();
		void unload();
	} core;
};

#endif
