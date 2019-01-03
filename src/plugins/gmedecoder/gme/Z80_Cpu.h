// Z80 CPU emulator

// $package
#ifndef Z80_CPU_H
#define Z80_CPU_H

#include "blargg_endian.h"

class Z80_Cpu {
public:
	typedef int time_t;
	typedef int addr_t;
	typedef BOOST::uint8_t byte;
	
	// Clears registers and maps all pages to unmapped
	void reset( void* unmapped_write, void const* unmapped_read );
	
	// TODO: split mapping out of CPU
	
	// Maps memory. Start and size must be multiple of page_size.
	enum { page_bits = 10 };
	enum { page_size = 1 << page_bits };
	void map_mem( addr_t addr, int size, void* write, void const* read );
	void map_mem( addr_t addr, int size, void* read_write );
	
	// Maps address to pointer to that byte
	byte      * write( addr_t addr );
	byte const* read(  addr_t addr );
	
	// Time of beginning of next instruction
	time_t time() const             { return cpu_state->time + cpu_state->base; }
	
	// Alter current time
	void set_time( time_t t )       { cpu_state->time = t - cpu_state->base; }
	void adjust_time( int delta )   { cpu_state->time += delta; }
	
	#if BLARGG_BIG_ENDIAN
		struct regs_t { byte b,c, d,e, h,l, flags,a; };
	#else
		struct regs_t { byte c,b, e,d, l,h, a,flags; };
	#endif
	BLARGG_STATIC_ASSERT( sizeof (regs_t) == 8 );
	
	struct pairs_t { BOOST::uint16_t bc, de, hl, fa; };
	
	// Registers are not updated until run() returns
	struct registers_t {
		BOOST::uint16_t pc;
		BOOST::uint16_t sp;
		BOOST::uint16_t ix;
		BOOST::uint16_t iy;
		union {
			regs_t b; //  b.b, b.c, b.d, b.e, b.h, b.l, b.flags, b.a
			pairs_t w; // w.bc, w.de, w.hl. w.fa
		};
		union {
			regs_t b;
			pairs_t w;
		} alt;
		byte iff1;
		byte iff2;
		byte r;
		byte i;
		byte im;
	};
	//registers_t r; (below for efficiency)
	
	// can read this far past end of memory
	enum { cpu_padding = 0x100 };
	
	// Can read this many bytes past end of a page
	enum { page_padding = 4 };
	
	void set_end_time( time_t t );
public:
	Z80_Cpu();
	
	enum { page_count = 0x10000 / page_size };
	byte szpc [0x200];
	time_t end_time_;
	struct cpu_state_t {
		byte const* read  [page_count + 1];
		byte      * write [page_count + 1];
		time_t base;
		time_t time;
	};
	cpu_state_t* cpu_state; // points to cpu_state_ or a local copy within run()
	cpu_state_t cpu_state_;
	void set_page( int i, void* write, void const* read );
public:
	registers_t r;
};

#if BLARGG_NONPORTABLE
	#define Z80_CPU_OFFSET( addr ) (addr)
#else
	#define Z80_CPU_OFFSET( addr ) ((addr) & (Z80_Cpu::page_size - 1))
#endif

inline Z80_Cpu::byte* Z80_Cpu::write( addr_t addr )
{
	return cpu_state->write [(unsigned) addr >> page_bits] + Z80_CPU_OFFSET( addr );
}

inline Z80_Cpu::byte const* Z80_Cpu::read( addr_t addr )
{
	return cpu_state->read [(unsigned) addr >> page_bits] + Z80_CPU_OFFSET( addr );
}

inline void Z80_Cpu::map_mem( addr_t addr, int size, void* p )
{
	map_mem( addr, size, p, p );
}

inline void Z80_Cpu::set_end_time( time_t t )
{
	time_t delta = cpu_state->base - t;
	cpu_state->base = t;
	cpu_state->time += delta;
}

#endif
