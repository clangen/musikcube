// Common interface for loading file data from various sources

// Game_Music_Emu $vers
#ifndef GME_LOADER_H
#define GME_LOADER_H

#include "blargg_common.h"
#include "Data_Reader.h"

class Gme_Loader {
public:

	// Each loads game music data from a file and returns an error if
	// file is wrong type or is seriously corrupt. Minor problems are
	// reported using warning().
	
	// Loads from file on disk
	blargg_err_t load_file( const char path [] );
	
	// Loads from custom data source (see Data_Reader.h)
	blargg_err_t load( Data_Reader& );
	
	// Loads from file already read into memory. Object might keep pointer to
	// data; if it does, you MUST NOT free it until you're done with the file.
	blargg_err_t load_mem( void const* data, long size );
	
	// Most recent warning string, or NULL if none. Clears current warning after
	// returning.
	const char* warning();
	
	// Unloads file from memory
	virtual void unload();
	
	virtual ~Gme_Loader();
	
protected:
	typedef BOOST::uint8_t byte;
	
	// File data in memory, or 0 if data was loaded with load_()
	byte const* file_begin() const      { return file_begin_; }
	byte const* file_end() const        { return file_end_; }
	int file_size() const               { return (int) (file_end_ - file_begin_); }
	
	// Sets warning string
	void set_warning( const char s [] ) { warning_ = s; }
	
	// At least one must be overridden
	virtual blargg_err_t load_( Data_Reader& ); // default loads then calls load_mem_()
	virtual blargg_err_t load_mem_( byte const data [], int size ); // use data in memory
	
	// Optionally overridden
	virtual void pre_load()             { unload(); } // called before load_()/load_mem_()
	virtual blargg_err_t post_load()    { return blargg_ok; } // called after load_()/load_mem_() succeeds
	
private:
	// noncopyable
	Gme_Loader( const Gme_Loader& );
	Gme_Loader& operator = ( const Gme_Loader& );
	
// Implementation
public:
	Gme_Loader();
	BLARGG_DISABLE_NOTHROW
	
	blargg_vector<byte> file_data; // used only when loading from file to load_mem_()
	byte const* file_begin_;
	byte const* file_end_;
	const char* warning_;
	
	blargg_err_t load_mem_wrapper( byte const [], int );
	blargg_err_t post_load_( blargg_err_t err );
};

// Files are read with GME_FILE_READER. Default supports gzip if zlib is available.
#ifndef GME_FILE_READER
	#ifdef HAVE_ZLIB_H
		#define GME_FILE_READER Gzip_File_Reader
	#else
		#define GME_FILE_READER Std_File_Reader
	#endif
#elif defined (GME_FILE_READER_INCLUDE)
	#include GME_FILE_READER_INCLUDE
#endif

inline const char* Gme_Loader::warning()
{
	const char* s = warning_;
	warning_ = NULL;
	return s;
}

#endif
