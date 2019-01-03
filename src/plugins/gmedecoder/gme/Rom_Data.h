// Manages ROM data loaded from file in an efficient manner

// Game_Music_Emu $vers
#ifndef ROM_DATA_H
#define ROM_DATA_H

#include "blargg_common.h"
#include "Data_Reader.h"

/* Loads a ROM file into memory and allows access to it in page-sized chunks.

* ROM file consists of header followed by ROM data. Instead of storing the entire
ROM contents, the file only stores the occupied portion, with the bytes before and
after that cleared to some value. The size and format of the header is up to the
caller, as is the starting address of the ROM data following it. File loading is
performed with a single read, rather than two or more that might otherwise be
required.

* Once ROM data is loaded and its address specified, a pointer to any "page" can
be obtained. ROM data is mirrored using smallest power of 2 that contains it.
Addresses not aligned to pages can also be used, but this might cause unexpected
results.

Example with file data of size 0x0C put at address 0x0F, with page size of 8:

---------------0123456789AB--------------------0123456789AB---------...
^       ^       ^       ^       ^       ^       ^       ^       ^   
0     0x08     0x10    0x18    0x20    0x28    0x30    0x38    0x40

at_addr(0x00) = pointer to 8 bytes of fill.
at_addr(0x08) = pointer to 7 bytes of fill, followed by first byte of file.
at_addr(0x10) = pointer to next 8 bytes of file.
at_addr(0x18) = pointer to last 3 bytes of file, followed by 5 bytes of fill.
at_addr(0x20) = pointer to 8 bytes of fill.
at_addr(0x28) = pointer to 7 bytes of fill, followed by first byte of file.
etc. */

class Rom_Data {
	enum { pad_extra = 8 };
public:
	typedef unsigned char byte;
	
	// Page_size should be a power of 2
	Rom_Data( int page_size );
	
	// Loads file into memory, then copies header to *header_out and fills
	// unmapped bank and file data padding with fill. Returns blargg_err_file_type
	// if in.remain() <= header_size.
	blargg_err_t load( Data_Reader& in, int header_size, void* header_out, int fill );
	
	// Below, "file data" refers to data AFTER the header
	
	// Size of file data
	int file_size() const               { return file_size_; }
	
	// Pointer to beginning of file data
	byte      * begin()                 { return rom.begin() + pad_size; }
	byte const* begin() const           { return rom.begin() + pad_size; }
	
	// Pointer to unmapped page cleared with fill value
	byte* unmapped()                    { return rom.begin(); }
	
	// Sets address that file data will start at. Must be set before using following
	// functions, and cannot be set more than once.
	void set_addr( int addr );
	
	// Address of first empty page (file size + addr rounded up to multiple of page_size)
	int size() const                    { return rom.size() - pad_extra + rom_addr; }
	
	// Masks address to nearest power of two greater than size()
	int mask_addr( int addr ) const     { return addr & mask; }
	
	// Pointer to page beginning at addr, or unmapped() if outside data.
	// Mirrored using mask_addr().
	byte* at_addr( int addr );
	
	// Frees memory
	void clear();

// Implementation
public:
	~Rom_Data();

protected:
	blargg_vector<byte> rom;
	int mask;
	int rom_addr;
	int const pad_size;
	int file_size_;
	
	blargg_err_t load_( Data_Reader& in, int header_size, int file_offset );
};

#endif
