/* Loads and plays video game music files into sample buffer */

/* Game_Music_Emu $vers */
#ifndef GME_H
#define GME_H

#ifdef __cplusplus
	extern "C" {
#endif

/* Pointer to error, or NULL if function was successful. See Errors below. */
#ifndef gme_err_t /* (#ifndef allows better testing of library) */
	typedef const char* gme_err_t;
#endif

/* First parameter of most functions is gme_t*, or const gme_t* if nothing is
changed. */
typedef struct gme_t gme_t;

/* Boolean; false = 0, true = 1 */
typedef int gme_bool;


/******** Basic operations ********/

/* Opens game music file and points *out at it. If error, sets *out to NULL. */
gme_err_t gme_open_file( const char path [], gme_t** out, int sample_rate );

/* Number of tracks */
int gme_track_count( const gme_t* );

/* Starts a track, where 0 is the first track. Requires that 0 <= index < gme_track_count(). */
gme_err_t gme_start_track( gme_t*, int index );

/* Generates 'count' 16-bit signed samples info 'out'. Output is in stereo, so count
must be even. */
gme_err_t gme_play( gme_t*, int count, short out [] );

/* Closes file and frees memory. OK to pass NULL. */
void gme_delete( gme_t* );


/******** Track position/length ********/

/* Sets time to start fading track out. Once fade ends track_ended() returns true.
Fade time can be changed while track is playing. */
void gme_set_fade( gme_t*, int start_msec, int length_msec );

/* True if a track has reached its end */
gme_bool gme_track_ended( const gme_t* );

/* Number of milliseconds played since beginning of track (1000 = one second) */
int gme_tell( const gme_t* );

/* Seeks to new time in track. Seeking backwards or far forward can take a while. */
gme_err_t gme_seek( gme_t*, int msec );

/* Skips the specified number of samples. */
gme_err_t gme_skip( gme_t*, int samples );


/******** Informational ********/

/* Use in place of sample rate for open/load if you only need to get track
information from a music file */
enum { gme_info_only = -1 };

/* Most recent warning string, or NULL if none. Clears current warning after returning.
Warning is also cleared when loading a file and starting a track. */
const char* gme_warning( gme_t* );

/* Loads m3u playlist file (must be done after loading music) */
gme_err_t gme_load_m3u( gme_t*, const char path [] );

/* Clears any loaded m3u playlist and any internal playlist that the music format
supports (NSFE for example). */
void gme_clear_playlist( gme_t* );

/* Passes back pointer to information for a particular track (length, name, author, etc.).
Must be freed after use. */
typedef struct gme_info_t gme_info_t;
gme_err_t gme_track_info( const gme_t*, gme_info_t** out, int track );

gme_err_t gme_set_track_info( gme_t*, const gme_info_t* in, int track );
        
/* Frees track information */
void gme_free_info( gme_info_t* );

struct gme_info_t
{
	/* times in milliseconds; -1 if unknown */
	int length;         /* total length, if file specifies it */
	int intro_length;   /* length of song up to looping section */
	int loop_length;    /* length of looping section */
	
	/* Length if available, otherwise intro_length+loop_length*2 if available,
	otherwise a default of 150000 (2.5 minutes). */
	int play_length;
	
	int i4,i5,i6,i7,i8,i9,i10,i11,i12,i13,i14,i15; /* reserved */
	
	/* empty string ("") if not available */
	const char* system;
	const char* game;
	const char* song;
	const char* author;
	const char* copyright;
	const char* comment;
	const char* dumper;
	
	const char *s7,*s8,*s9,*s10,*s11,*s12,*s13,*s14,*s15; /* reserved */
};


/******** Advanced playback ********/

/* Disables automatic end-of-track detection and skipping of silence at beginning
if ignore is true */
void gme_ignore_silence( gme_t*, gme_bool ignore );

/* Adjusts song tempo, where 1.0 = normal, 0.5 = half speed, 2.0 = double speed, etc.
Track length as returned by track_info() ignores tempo (assumes it's 1.0). */
void gme_set_tempo( gme_t*, double tempo );

/* Number of voices used by currently loaded file */
int gme_voice_count( const gme_t* );

/* Name of voice i, from 0 to gme_voice_count() - 1 */
const char* gme_voice_name( const gme_t*, int i );

/* Mutes/unmutes single voice i, where voice 0 is first voice */
void gme_mute_voice( gme_t*, int index, gme_bool mute );

/* Sets muting state of ALL voices at once using a bit mask, where -1 mutes all
voices, 0 unmutes them all, 0x01 mutes just the first voice, etc. */
void gme_mute_voices( gme_t*, int muting_mask );

/* Frequency equalizer parameters (see gme.txt) */
typedef struct gme_equalizer_t
{
	double treble; /* -50.0 = muffled, 0 = flat, +5.0 = extra-crisp */
	double bass;   /* 1 = full bass, 90 = average, 16000 = almost no bass */
	
	double d2,d3,d4,d5,d6,d7,d8,d9; /* reserved */
} gme_equalizer_t;

/* Gets current frequency equalizer parameters */
void gme_equalizer( const gme_t*, gme_equalizer_t* out );

/* Changes frequency equalizer parameters */
void gme_set_equalizer( gme_t*, gme_equalizer_t const* eq );



/******** Effects processor ********/

/* Adds stereo surround and echo to music that's usually mono or has little
stereo. Has no effect on GYM, SPC, and Sega Genesis VGM music. */

/* Simplified control using a single value, where 0.0 = off and 1.0 = maximum */
void gme_set_stereo_depth( gme_t*, double depth );

struct gme_effects_t
{
	double echo;    /* Amount of echo, where 0.0 = none, 1.0 = lots */
	double stereo;  /* Separation, where 0.0 = mono, 1.0 = hard left and right */
	
	double d2,d3,d4,d5,d6,d7; /* reserved */
	
	gme_bool enabled; /* If 0, no effects are added */
	gme_bool surround;/* If 1, some channels are put in "back", using phase inversion */
	
	int i1,i3,i4,i5,i6,i7; /* reserved */
};
typedef struct gme_effects_t gme_effects_t;

/* Sets effects configuration, or disables effects if NULL */
void gme_set_effects( gme_t*, gme_effects_t const* );

/* Passes back current effects configuration */
void gme_effects( const gme_t*, gme_effects_t* out );


/******** Game music types ********/

/* Music file type identifier. Can also hold NULL. */
typedef const struct gme_type_t_* gme_type_t;

/* Type of this emulator */
gme_type_t gme_type( const gme_t* );

/* Pointer to array of all music types, with NULL entry at end. Allows a player linked
to this library to support new music types without having to be updated. */
gme_type_t const* gme_type_list();

/* Name of game system for this music file type */
const char* gme_type_system( gme_type_t );

/* True if this music file type supports multiple tracks */
gme_bool gme_type_multitrack( gme_type_t );


/******** Advanced file loading ********/

/* Same as gme_open_file(), but uses file data already in memory. Makes copy of data. */
gme_err_t gme_open_data( void const* data, long size, gme_t** emu_out, int sample_rate );

/* Determines likely game music type based on first four bytes of file. Returns
string containing proper file suffix ("NSF", "SPC", etc.) or "" if file header
is not recognized. */
const char* gme_identify_header( void const* header );

/* Gets corresponding music type for file path or extension passed in. */
gme_type_t gme_identify_extension( const char path_or_extension [] );

/* Determines file type based on file's extension or header (if extension isn't recognized).
Sets *type_out to type, or 0 if unrecognized or error. */
gme_err_t gme_identify_file( const char path [], gme_type_t* type_out );

/* Creates new emulator and sets sample rate. Returns NULL if out of memory. If you only need
track information, pass gme_info_only for sample_rate. */
gme_t* gme_new_emu( gme_type_t, int sample_rate );

/* Loads music file into emulator */
gme_err_t gme_load_file( gme_t*, const char path [] );

/* Loads music file from memory into emulator. Makes a copy of data passed. */
gme_err_t gme_load_data( gme_t*, void const* data, long size );

/* Loads music file using custom data reader function that will be called to
read file data. Most emulators load the entire file in one read call. */
typedef gme_err_t (*gme_reader_t)( void* your_data, void* out, long count );
gme_err_t gme_load_custom( gme_t*, gme_reader_t, long file_size, void* your_data );

/* Loads m3u playlist file from memory (must be done after loading music) */
gme_err_t gme_load_m3u_data( gme_t*, void const* data, long size );

        
/******** Saving ********/
typedef gme_err_t (*gme_writer_t)( void* your_data, void const* in, long count );
gme_err_t gme_save( gme_t const*, gme_writer_t, void* your_data );

/******** User data ********/

/* Sets/gets pointer to data you want to associate with this emulator.
You can use this for whatever you want. */
void  gme_set_user_data( gme_t*, void* new_user_data );
void* gme_user_data( const gme_t* );

/* Registers cleanup function to be called when deleting emulator, or NULL to
clear it. Passes user_data when calling cleanup function. */
typedef void (*gme_user_cleanup_t)( void* user_data );
void gme_set_user_cleanup( gme_t*, gme_user_cleanup_t func );


/******** Errors ********/

/* Internally, a gme_err_t is a const char* that points to a normal C string.
This means that other strings can be passed to the following functions. In the
descriptions below, these other strings are referred to as being not gme_err_t
strings. */

/* Error string associated with err. Returns "" if err is NULL. Returns err
unchanged if it isn't a gme_err_t string. */
const char* gme_err_str( gme_err_t err );

/* Details of error beyond main cause, or "" if none or err is NULL. Returns
err unchanged if it isn't a gme_err_t string. */
const char* gme_err_details( gme_err_t err );

/* Numeric code corresponding to err. Returns gme_ok if err is NULL. Returns
gme_err_generic if err isn't a gme_err_t string. */
int gme_err_code( gme_err_t err );

enum {
	gme_ok               =    0,/* Successful call. Guaranteed to be zero. */
	gme_err_generic      = 0x01,/* Error of unspecified type */
	gme_err_memory       = 0x02,/* Out of memory */
	gme_err_caller       = 0x03,/* Caller violated requirements of function */
	gme_err_internal     = 0x04,/* Internal problem, corruption, etc. */
	gme_err_limitation   = 0x05,/* Exceeded program limit */
	
	gme_err_file_missing = 0x20,/* File not found at specified path */
	gme_err_file_read    = 0x21,/* Couldn't open file for reading */
	gme_err_file_io      = 0x23,/* Read/write error */
	gme_err_file_eof     = 0x25,/* Tried to read past end of file */
	
	gme_err_file_type    = 0x30,/* File is of wrong type */
	gme_err_file_feature = 0x32,/* File requires unsupported feature */
	gme_err_file_corrupt = 0x33 /* File is corrupt */
};

/* gme_err_t corresponding to numeric code. Note that this might not recover
the original gme_err_t before it was converted to a numeric code; in
particular, gme_err_details(gme_code_to_err(code)) will be "" in most cases. */
gme_err_t gme_code_to_err( int code );



/* Deprecated */
typedef gme_t Music_Emu;

#ifdef __cplusplus
	}
#endif

#endif
