// Common interface for track information

// Game_Music_Emu $vers
#ifndef GME_FILE_H
#define GME_FILE_H

#include "gme.h"
#include "Gme_Loader.h"
#include "M3u_Playlist.h"

struct track_info_t
{
	int track_count;
	
	/* times in milliseconds; -1 if unknown */
	int length;         /* total length, if file specifies it */
	int intro_length;   /* length of song up to looping section */
	int loop_length;    /* length of looping section */
	int fade_length;
	int repeat_count;
	
	/* Length if available, otherwise intro_length+loop_length*2 if available,
	otherwise a default of 150000 (2.5 minutes). */
	int play_length;
	
	/* empty string if not available */
	char system    [256];
	char game      [256];
	char song      [256];
	char author    [256];
	char composer  [256];
	char engineer  [256];
	char sequencer [256];
	char tagger    [256];
	char copyright [256];
	char date      [256];
	char comment   [256];
	char dumper    [256];
	char disc      [256];
	char track     [256];
	char ost       [256];
};
enum { gme_max_field = 255 };

class Gme_File : public Gme_Loader {
public:
	// Type of emulator. For example if this returns gme_nsfe_type, this object
	// is an NSFE emulator, and you can downcast to an Nsfe_Emu* if necessary.
	gme_type_t type() const;
	
	// Loads an m3u playlist. Must be done AFTER loading main music file.
	blargg_err_t load_m3u( const char path [] );
	blargg_err_t load_m3u( Data_Reader& in );
	
	// Clears any loaded m3u playlist and any internal playlist that the music
	// format supports (NSFE for example).
	void clear_playlist();
	
	// Number of tracks or 0 if no file has been loaded
	int track_count() const;
	
	// Gets information for a track (length, name, author, etc.)
	// See gme.h for definition of struct track_info_t.
	blargg_err_t track_info( track_info_t* out, int track ) const;

// User data/cleanup
	
	// Sets/gets pointer to data you want to associate with this emulator.
	// You can use this for whatever you want.
	void set_user_data( void* p )       { user_data_ = p; }
	void* user_data() const             { return user_data_; }
	
	// Registers cleanup function to be called when deleting emulator, or NULL to
	// clear it. Passes user_data to cleanup function.
	void set_user_cleanup( gme_user_cleanup_t func ) { user_cleanup_ = func; }

public:
	Gme_File();
	~Gme_File();

protected:
	// Services
	void set_type( gme_type_t t )               { type_ = t; }
	void set_track_count( int n )               { track_count_ = raw_track_count_ = n; }
	
	// Must be overridden
	virtual blargg_err_t track_info_( track_info_t* out, int track ) const BLARGG_PURE( ; )
	
	// Optionally overridden
	virtual void clear_playlist_() { }

protected: // Gme_Loader overrides
	virtual void unload();
	virtual blargg_err_t post_load();

protected:
	blargg_err_t remap_track_( int* track_io ) const; // need by Music_Emu
private:
	gme_type_t type_;
	void* user_data_;
	gme_user_cleanup_t user_cleanup_;
	int track_count_;
	int raw_track_count_;
	M3u_Playlist playlist;
	char playlist_warning [64];
	
	blargg_err_t load_m3u_( blargg_err_t );

public:
	// track_info field copying
	enum { max_field_ = 255 };
	static void copy_field_( char out [], const char* in );
	static void copy_field_( char out [], const char* in, int len );
};

struct gme_type_t_
{
	const char* system;      /* name of system this music file type is generally for */
	int track_count;         /* non-zero for formats with a fixed number of tracks */
	Music_Emu* (*new_emu)(); /* Create new emulator for this type (C++ only) */
	Music_Emu* (*new_info)();/* Create new info reader for this type (C++ only) */
	
	/* internal */
	const char* extension_;
	int flags_;
};

/* Emulator type constants for each supported file type */
extern const gme_type_t_
	gme_ay_type [1],
	gme_gbs_type [1],
	gme_gym_type [1],
	gme_hes_type [1],
	gme_kss_type [1],
	gme_nsf_type [1],
	gme_nsfe_type [1],
	gme_sap_type [1],
    gme_sfm_type [1],
	gme_sgc_type [1],
	gme_spc_type [1],
	gme_vgm_type [1],
	gme_vgz_type [1];

#define GME_COPY_FIELD( in, out, name ) \
	{ Gme_File::copy_field_( out->name, in.name, sizeof in.name ); }

inline gme_type_t Gme_File::type() const            { return type_; }

inline int Gme_File::track_count() const            { return track_count_; }

inline blargg_err_t Gme_File::track_info_( track_info_t*, int ) const { return blargg_ok; }

#endif
