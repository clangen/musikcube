// PC Engine CPU emulator for use with HES music files

// $package
#ifndef HES_CPU_H
#define HES_CPU_H

#include "blargg_common.h"

class Hes_Cpu {
public:
	typedef BOOST::uint8_t byte;
	typedef int time_t;
	typedef int addr_t;
	enum { future_time = INT_MAX/2 + 1 };
	
	void reset();
	
	enum { page_bits = 13 };
	enum { page_size = 1 << page_bits };
	enum { page_count = 0x10000 / page_size };
	void set_mmr( int reg, int bank, void const* code );
	
	byte const* get_code( addr_t );
	
	// NOT kept updated during emulation.
	struct registers_t {
		BOOST::uint16_t pc;
		byte a;
		byte x;
		byte y;
		byte flags;
		byte sp;
	};
	registers_t r;
	
	// page mapping registers
	byte mmr [page_count + 1];
	
	// Time of beginning of next instruction to be executed
	time_t time() const             { return cpu_state->time + cpu_state->base; }
	void set_time( time_t t )       { cpu_state->time = t - cpu_state->base; }
	void adjust_time( int delta )   { cpu_state->time += delta; }
	
	// Clocks past end (negative if before)
	int time_past_end() const           { return cpu_state->time; }
	
	// Time of next IRQ
	time_t irq_time() const         { return irq_time_; }
	void set_irq_time( time_t );
	
	// Emulation stops once time >= end_time
	time_t end_time() const         { return end_time_; }
	void set_end_time( time_t );
	
	// Subtracts t from all times
	void end_frame( time_t t );
	
	// Can read this many bytes past end of a page
	enum { cpu_padding = 8 };
	
private:
	// noncopyable
	Hes_Cpu( const Hes_Cpu& );
	Hes_Cpu& operator = ( const Hes_Cpu& );


// Implementation
public:
	Hes_Cpu() { cpu_state = &cpu_state_; }
	enum { irq_inhibit_mask = 0x04 };

	struct cpu_state_t {
		byte const* code_map [page_count + 1];
		time_t base;
		int time;
	};
	cpu_state_t* cpu_state; // points to cpu_state_ or a local copy
	cpu_state_t cpu_state_;
	time_t irq_time_;
	time_t end_time_;
	
private:
	void set_code_page( int, void const* );
	inline void update_end_time( time_t end, time_t irq );
};

#define HES_CPU_PAGE( addr ) ((unsigned) (addr) >> Hes_Cpu::page_bits)

#if BLARGG_NONPORTABLE
	#define HES_CPU_OFFSET( addr ) (addr)
#else
	#define HES_CPU_OFFSET( addr ) ((addr) & (Hes_Cpu::page_size - 1))
#endif

inline BOOST::uint8_t const* Hes_Cpu::get_code( addr_t addr )
{
	return cpu_state_.code_map [HES_CPU_PAGE( addr )] + HES_CPU_OFFSET( addr );
}

inline void Hes_Cpu::update_end_time( time_t end, time_t irq )
{
	if ( end > irq && !(r.flags & irq_inhibit_mask) )
		end = irq;
	
	cpu_state->time += cpu_state->base - end;
	cpu_state->base = end;
}

inline void Hes_Cpu::set_irq_time( time_t t )
{
	irq_time_ = t;
	update_end_time( end_time_, t );
}

inline void Hes_Cpu::set_end_time( time_t t )
{
	end_time_ = t;
	update_end_time( t, irq_time_ );
}

inline void Hes_Cpu::end_frame( time_t t )
{
	assert( cpu_state == &cpu_state_ );
	cpu_state_.base -= t;
	if ( irq_time_ < future_time ) irq_time_ -= t;
	if ( end_time_ < future_time ) end_time_ -= t;
}

inline void Hes_Cpu::set_mmr( int reg, int bank, void const* code )
{
	assert( (unsigned) reg <= page_count ); // allow page past end to be set
	assert( (unsigned) bank < 0x100 );
	mmr [reg] = bank;
	byte const* p = STATIC_CAST(byte const*,code) - HES_CPU_OFFSET( reg << page_bits );
	cpu_state->code_map [reg] = p;
	cpu_state_.code_map [reg] = p;
}

#endif
