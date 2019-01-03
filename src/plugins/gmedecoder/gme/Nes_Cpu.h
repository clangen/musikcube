// NES CPU emulator

// $package
#ifndef NES_CPU_H
#define NES_CPU_H

#include "blargg_common.h"

class Nes_Cpu {
public:
	typedef BOOST::uint8_t byte;
	typedef int time_t;
	typedef int addr_t;
	enum { future_time = INT_MAX/2 + 1 };
	
	// Clears registers and maps all pages to unmapped_page
	void reset( void const* unmapped_page = NULL );
	
	// Maps code memory (memory accessed via the program counter). Start and size
	// must be multiple of page_size. If mirror_size is non-zero, the first
	// mirror_size bytes are repeated over the range. mirror_size must be a
	// multiple of page_size.
	enum { page_bits = 11 };
	enum { page_size = 1 << page_bits };
	void map_code( addr_t start, int size, void const* code, int mirror_size = 0 );
	
	// Accesses emulated memory as CPU does
	byte const* get_code( addr_t ) const;
	
	// NES 6502 registers. NOT kept updated during emulation.
	struct registers_t {
		BOOST::uint16_t pc;
		byte a;
		byte x;
		byte y;
		byte flags;
		byte sp;
	};
	registers_t r;
	
	// Time of beginning of next instruction to be executed
	time_t time() const             { return cpu_state->time + cpu_state->base; }
	void set_time( time_t t )       { cpu_state->time = t - cpu_state->base; }
	void adjust_time( int delta )   { cpu_state->time += delta; }
	
	// Clocks past end (negative if before)
	int time_past_end() const       { return cpu_state->time; }
	
	// Time of next IRQ
	time_t irq_time() const         { return irq_time_; }
	void set_irq_time( time_t );
	
	// Emulation stops once time >= end_time
	time_t end_time() const         { return end_time_; }
	void set_end_time( time_t );
	
	// Number of unimplemented instructions encountered and skipped
	void clear_error_count()        { error_count_ = 0; }
	unsigned error_count() const    { return error_count_; }
	void count_error()              { error_count_++; }
	
	// Unmapped page should be filled with this
	enum { halt_opcode = 0x22 };
	
	enum { irq_inhibit_mask = 0x04 };
	
	// Can read this many bytes past end of a page
	enum { cpu_padding = 8 };

private:
	// noncopyable
	Nes_Cpu( const Nes_Cpu& );
	Nes_Cpu& operator = ( const Nes_Cpu& );


// Implementation
public:
	Nes_Cpu() { cpu_state = &cpu_state_; }
	enum { page_count = 0x10000 >> page_bits };
	
	struct cpu_state_t {
		byte const* code_map [page_count + 1];
		time_t base;
		int time;
	};
	cpu_state_t* cpu_state; // points to cpu_state_ or a local copy
	cpu_state_t cpu_state_;
	time_t irq_time_;
	time_t end_time_;
	unsigned error_count_;
	
private:
	void set_code_page( int, void const* );
	inline void update_end_time( time_t end, time_t irq );
};

#define NES_CPU_PAGE( addr ) ((unsigned) (addr) >> Nes_Cpu::page_bits)

#if BLARGG_NONPORTABLE
	#define NES_CPU_OFFSET( addr ) (addr)
#else
	#define NES_CPU_OFFSET( addr ) ((addr) & (Nes_Cpu::page_size - 1))
#endif

inline BOOST::uint8_t const* Nes_Cpu::get_code( addr_t addr ) const
{
	return cpu_state_.code_map [NES_CPU_PAGE( addr )] + NES_CPU_OFFSET( addr );
}

inline void Nes_Cpu::update_end_time( time_t end, time_t irq )
{
	if ( end > irq && !(r.flags & irq_inhibit_mask) )
		end = irq;
	
	cpu_state->time += cpu_state->base - end;
	cpu_state->base = end;
}

inline void Nes_Cpu::set_irq_time( time_t t )
{
	irq_time_ = t;
	update_end_time( end_time_, t );
}

inline void Nes_Cpu::set_end_time( time_t t )
{
	end_time_ = t;
	update_end_time( t, irq_time_ );
}   

#endif
