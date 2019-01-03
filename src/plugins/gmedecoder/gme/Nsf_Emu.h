// Nintendo NES/Famicom NSF music file emulator

// Game_Music_Emu $vers
#ifndef NSF_EMU_H
#define NSF_EMU_H

#include "Classic_Emu.h"
#include "Nsf_Core.h"

void hash_nsf_file( Nsf_Core::header_t const& h, unsigned char const* data, int data_size, Music_Emu::Hash_Function& out );

class Nsf_Emu : public Classic_Emu {
public:
	// Equalizer profiles for US NES and Japanese Famicom
	static equalizer_t const nes_eq;
	static equalizer_t const famicom_eq;
	
	// NSF file header (see Nsf_Impl.h)
	typedef Nsf_Core::header_t header_t;
	
	// Header for currently loaded file
	header_t const& header() const { return core_.header(); }

	blargg_err_t hash_( Hash_Function& ) const;
	
	static gme_type_t static_type() { return gme_nsf_type; }
	
	Nsf_Core& core() { return core_; }
	
public:
	Nsf_Emu();
	~Nsf_Emu();
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
	enum { max_voices = 32 };
	const char* voice_names_ [32];
	int voice_types_ [32];
	int voice_count_;
	Nsf_Core core_;
	
	blargg_err_t init_sound();
	void append_voices( const char* const names [], int const types [], int count );
};

#endif
