// Nintendo NES/Famicom NSFE music file emulator

// Game_Music_Emu $vers
#ifndef NSFE_EMU_H
#define NSFE_EMU_H

#include "blargg_common.h"
#include "Nsf_Emu.h"
class Nsfe_Emu;

// Allows reading info from NSFE file without creating emulator
class Nsfe_Info {
public:
	blargg_err_t load( Data_Reader&, Nsfe_Emu* );
	
	struct info_t : Nsf_Emu::header_t
	{
		char game      [256];
		char author    [256];
		char copyright [256];
		char dumper    [256];
	} info;

	blargg_vector<unsigned char> data;

	void disable_playlist( bool = true );
	
	blargg_err_t track_info_( track_info_t* out, int track ) const;
	
	int remap_track( int i ) const;
	
	void unload();
	
// Implementation
public:
	Nsfe_Info();
	~Nsfe_Info();
	BLARGG_DISABLE_NOTHROW
private:
	blargg_vector<char> track_name_data;
	blargg_vector<const char*> track_names;
	blargg_vector<unsigned char> playlist;
	blargg_vector<char [4]> track_times;
	int actual_track_count_;
	bool playlist_disabled;
};

class Nsfe_Emu : public Nsf_Emu {
public:
	static gme_type_t static_type() { return gme_nsfe_type; }
	
	struct header_t { char tag [4]; };


// Implementation
public:
	Nsfe_Emu();
	~Nsfe_Emu();
	virtual void unload();

protected:
	virtual blargg_err_t load_( Data_Reader& );
	virtual blargg_err_t track_info_( track_info_t*, int track ) const;
	virtual blargg_err_t start_track_( int );
	virtual void clear_playlist_();

private:
	Nsfe_Info info;
	
	void disable_playlist_( bool b );
	friend class Nsfe_Info;
};

#endif
