// Nintendo Game Boy CPU emulator

// Game_Music_Emu $vers
#ifndef GB_CPU_H
#define GB_CPU_H

#include "blargg_common.h"

class Gb_Cpu {
public:
	typedef int addr_t;
	typedef BOOST::uint8_t byte;
	
	enum { mem_size = 0x10000 };
	
	// Clears registers and map all pages to unmapped
	void reset( void* unmapped = NULL );
	
	// Maps code memory (memory accessed via the program counter). Start and size
	// must be multiple of page_size.
	enum { page_bits = 13 };
	enum { page_size = 1 << page_bits };
	void map_code( addr_t start, int size, void* code );
	
	// Accesses emulated memory as CPU does
	byte* get_code( addr_t );
	
	// Game Boy Z-80 registers. NOT kept updated during emulation.
	struct core_regs_t {
		BOOST::uint16_t bc, de, hl, fa;
	};
	
	struct registers_t : core_regs_t {
		int pc; // more than 16 bits to allow overflow detection
		BOOST::uint16_t sp;
	};
	registers_t r;
	
	// Base address for RST vectors, to simplify GBS player (normally 0)
	addr_t rst_base;
	
	// Current time.
	int time() const { return cpu_state->time; }
	
	// Changes time. Must not be called during emulation.
	// Should be negative, because emulation stops once it becomes >= 0.
	void set_time( int t ) { cpu_state->time = t; }
	
	// Emulator reads this many bytes past end of a page
	enum { cpu_padding = 8 };

	
// Implementation
public:
	Gb_Cpu() : rst_base( 0 ) { cpu_state = &cpu_state_; }
	enum { page_count = mem_size >> page_bits };
	
	struct cpu_state_t {
		byte* code_map [page_count + 1];
		int time;
	};
	cpu_state_t* cpu_state; // points to state_ or a local copy within run()
	cpu_state_t cpu_state_;
	
private:
	void set_code_page( int, void* );
};

#define GB_CPU_PAGE( addr ) ((unsigned) (addr) >> Gb_Cpu::page_bits)

#if BLARGG_NONPORTABLE
	#define GB_CPU_OFFSET( addr ) (addr)
#else
	#define GB_CPU_OFFSET( addr ) ((addr) & (Gb_Cpu::page_size - 1))
#endif

inline BOOST::uint8_t* Gb_Cpu::get_code( addr_t addr )
{
	return cpu_state_.code_map [GB_CPU_PAGE( addr )] + GB_CPU_OFFSET( addr );
}

#endif
