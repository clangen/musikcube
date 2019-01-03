// $package. http://www.slack.net/~ant/

#if 0
/* Define these macros in the source file before #including this file.
- Parameters might be expressions, so they are best evaluated only once,
though they NEVER have side-effects, so multiple evaluation is OK.
- Output parameters might be a multiple-assignment expression like "a=x",
so they must NOT be parenthesized.
- Except where noted, time() and related functions will NOT work
correctly inside a macro. TIME() is always correct, and FLUSH_TIME() and
CACHE_TIME() allow the time changing functions to work.
- Macros "returning" void may use a {} statement block. */

	// 0 <= addr <= 0xFFFF + page_size
	// time functions can be used
	int  READ_MEM(  addr_t );
	void WRITE_MEM( addr_t, int data );
	
	// 0 <= addr <= 0x1FF
	int  READ_LOW(  addr_t );
	void WRITE_LOW( addr_t, int data );

	// 0 <= addr <= 0xFFFF + page_size
	// Used by common instructions.
	int  READ_FAST(  addr_t, int& out );
	void WRITE_FAST( addr_t, int data );

	// 0 <= addr <= 2
	// ST0, ST1, ST2 instructions
	void WRITE_VDP( int addr, int data );

// The following can be used within macros:
	
	// Current time
	time_t TIME();
	
	// Allows use of time functions
	void FLUSH_TIME();
	
	// Must be used before end of macro if FLUSH_TIME() was used earlier
	void CACHE_TIME();

// Configuration (optional; commented behavior if defined)
	
	// Expanded just before beginning of code, to help debugger
	#define CPU_BEGIN void my_run_cpu() {
#endif

/* Copyright (C) 2003-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

// TODO: support T flag, including clearing it at appropriate times?

// all zero-page should really use whatever is at page 1, but that would
// reduce efficiency quite a bit
int const ram_addr = 0x2000;

void Hes_Cpu::reset()
{
	check( cpu_state == &cpu_state_ );
	cpu_state = &cpu_state_;
	
	cpu_state_.time = 0;
	cpu_state_.base = 0;
	irq_time_   = future_time;
	end_time_   = future_time;
	
	r.flags = 0x04;
	r.sp    = 0;
	r.pc    = 0;
	r.a     = 0;
	r.x     = 0;
	r.y     = 0;
	
	// Be sure "blargg_endian.h" has been #included
	blargg_verify_byte_order();
}

// Allows MWCW debugger to step through code properly
#ifdef CPU_BEGIN
	CPU_BEGIN
#endif

// Time
#define TIME()          (s_time + s.base)
#define FLUSH_TIME()    {s.time = s_time;}
#define CACHE_TIME()    {s_time = s.time;}

// Memory
#define READ_STACK          READ_LOW
#define WRITE_STACK         WRITE_LOW

#define CODE_PAGE( addr )   s.code_map [HES_CPU_PAGE( addr )]
#define CODE_OFFSET( addr ) HES_CPU_OFFSET( addr )
#define READ_CODE( addr )   CODE_PAGE( addr ) [CODE_OFFSET( addr )]

// Stack
#define SET_SP( v )     (sp = ((v) + 1) | 0x100)
#define GET_SP()        ((sp - 1) & 0xFF)
#define SP( o )         ((sp + (o - (o>0)*0x100)) | 0x100)

// Truncation
#define BYTE(  n ) ((BOOST::uint8_t ) (n)) /* (unsigned) n & 0xFF */
#define SBYTE( n ) ((BOOST::int8_t  ) (n)) /* (BYTE( n ) ^ 0x80) - 0x80 */
#define WORD(  n ) ((BOOST::uint16_t) (n)) /* (unsigned) n & 0xFFFF */

// Flags with hex value for clarity when used as mask.
// Stored in indicated variable during emulation.
int const n80 = 0x80; // nz
int const v40 = 0x40; // flags
//int const t20 = 0x20;
int const b10 = 0x10;
int const d08 = 0x08; // flags
int const i04 = 0x04; // flags
int const z02 = 0x02; // nz
int const c01 = 0x01; // c

#define IS_NEG (nz & 0x8080)

#define GET_FLAGS( out ) \
{\
	out = flags & (v40 | d08 | i04);\
	out += ((nz >> 8) | nz) & n80;\
	out += c >> 8 & c01;\
	if ( !BYTE( nz ) )\
		out += z02;\
}

#define SET_FLAGS( in ) \
{\
	flags = in & (v40 | d08 | i04);\
	c = nz = in << 8;\
	nz += ~in & z02;\
}

bool illegal_encountered = false;
{
	Hes_Cpu::cpu_state_t s = CPU.cpu_state_;
	CPU.cpu_state = &s;
	// even on x86, using s.time in place of s_time was slower
	int s_time = s.time;
	
	// registers
	int pc = CPU.r.pc;
	int a  = CPU.r.a;
	int x  = CPU.r.x;
	int y  = CPU.r.y;
	int sp;
	SET_SP( CPU.r.sp );
	
	// Flags
	int flags;
	int c;  // carry set if (c & 0x100) != 0
	int nz; // Z set if (nz & 0xFF) == 0, N set if (nz & 0x8080) != 0
	{
		int temp = CPU.r.flags;
		SET_FLAGS( temp );
	}
	
loop:
	
	#ifndef NDEBUG
	{
		time_t correct = CPU.end_time_;
		if ( !(flags & i04) && correct > CPU.irq_time_ )
			correct = CPU.irq_time_;
		check( s.base == correct );
		/*
		static int count;
		if ( count == 1844 ) Debugger();
		if ( s.base != correct ) dprintf( "%ld\n", count );
		count++;
		*/
	}
	#endif

	// Check all values
	check( (unsigned) sp - 0x100 < 0x100 );
	check( (unsigned) pc < 0x10000 + 0x100 ); // +0x100 so emulator can catch wrap-around
	check( (unsigned) a < 0x100 );
	check( (unsigned) x < 0x100 );
	check( (unsigned) y < 0x100 );
	
	// Read instruction
	byte const* instr = CODE_PAGE( pc );
	int opcode;
	
	if ( CODE_OFFSET(~0) == ~0 )
	{
		opcode = instr [pc];
		pc++;
		instr += pc;
	}
	else
	{
		instr += CODE_OFFSET( pc );
		opcode = *instr++;
		pc++;
	}
	
	// TODO: each reference lists slightly different timing values, ugh
	static byte const clock_table [256] =
	{// 0 1 2  3 4 5 6 7 8 9 A B C D E F
		1,7,3, 4,6,4,6,7,3,2,2,2,7,5,7,4,// 0
		2,7,7, 4,6,4,6,7,2,5,2,2,7,5,7,4,// 1
		7,7,3, 4,4,4,6,7,4,2,2,2,5,5,7,4,// 2
		2,7,7, 2,4,4,6,7,2,5,2,2,5,5,7,4,// 3
		7,7,3, 4,8,4,6,7,3,2,2,2,4,5,7,4,// 4
		2,7,7, 5,2,4,6,7,2,5,3,2,2,5,7,4,// 5
		7,7,2, 2,4,4,6,7,4,2,2,2,7,5,7,4,// 6
		2,7,7,17,4,4,6,7,2,5,4,2,7,5,7,4,// 7
		4,7,2, 7,4,4,4,7,2,2,2,2,5,5,5,4,// 8
		2,7,7, 8,4,4,4,7,2,5,2,2,5,5,5,4,// 9
		2,7,2, 7,4,4,4,7,2,2,2,2,5,5,5,4,// A
		2,7,7, 8,4,4,4,7,2,5,2,2,5,5,5,4,// B
		2,7,2,17,4,4,6,7,2,2,2,2,5,5,7,4,// C
		2,7,7,17,2,4,6,7,2,5,3,2,2,5,7,4,// D
		2,7,2,17,4,4,6,7,2,2,2,2,5,5,7,4,// E
		2,7,7,17,2,4,6,7,2,5,4,2,2,5,7,4 // F
	}; // 0x00 was 8
	
	// Update time
	if ( s_time >= 0 )
		goto out_of_time;
	
	#ifdef HES_CPU_LOG_H
		log_cpu( "new", pc - 1, opcode, instr [0], instr [1], instr [2],
				instr [3], instr [4], instr [5], a, x, y );
		//log_opcode( opcode );
	#endif

	s_time += clock_table [opcode];
	
	int data;
	data = *instr;
	
	switch ( opcode )
	{
// Macros

#define GET_MSB()       (instr [1])
#define ADD_PAGE( out ) (pc++, out = data + 0x100 * GET_MSB());
#define GET_ADDR()      GET_LE16( instr )

// TODO: is the penalty really always added? the original 6502 was much better
//#define PAGE_PENALTY( lsb )   (void) (s_time += (lsb) >> 8)
#define PAGE_PENALTY( lsb )

// Branch

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH_( cond, adj )\
{\
	pc++;\
	if ( !(cond) ) goto loop;\
	pc = (BOOST::uint16_t) (pc + SBYTE( data ));\
	s_time += adj;\
	goto loop;\
}

#define BRANCH( cond ) BRANCH_( cond, 2 )

	case 0xF0: // BEQ
		BRANCH( !BYTE( nz ) );
	
	case 0xD0: // BNE
		BRANCH( BYTE( nz ) );
	
	case 0x10: // BPL
		BRANCH( !IS_NEG );
	
	case 0x90: // BCC
		BRANCH( !(c & 0x100) )
	
	case 0x30: // BMI
		BRANCH( IS_NEG )
	
	case 0x50: // BVC
		BRANCH( !(flags & v40) )
	
	case 0x70: // BVS
		BRANCH( flags & v40 )
	
	case 0xB0: // BCS
		BRANCH( c & 0x100 )
	
	case 0x80: // BRA
	branch_taken:
		BRANCH_( true, 0 );
	
	case 0xFF:
		#ifdef IDLE_ADDR
			if ( pc == IDLE_ADDR + 1 )
				goto idle_done;
		#endif

		pc = (BOOST::uint16_t) pc;

	case 0x0F: // BBRn
	case 0x1F:
	case 0x2F:
	case 0x3F:
	case 0x4F:
	case 0x5F:
	case 0x6F:
	case 0x7F:
	case 0x8F: // BBSn
	case 0x9F:
	case 0xAF:
	case 0xBF:
	case 0xCF:
	case 0xDF:
	case 0xEF: {
		// Make two copies of bits, one negated
		int t = 0x101 * READ_LOW( data );
		t ^= 0xFF;
		pc++;
		data = GET_MSB();
		BRANCH( t & (1 << (opcode >> 4)) )
	}
	
	case 0x4C: // JMP abs
		pc = GET_ADDR();
		goto loop;
	
	case 0x7C: // JMP (ind+X)
		data += x;
	case 0x6C:{// JMP (ind)
		data += 0x100 * GET_MSB();
		pc = GET_LE16( &READ_CODE( data ) );
		goto loop;
	}
	
// Subroutine

	case 0x44: // BSR
		WRITE_STACK( SP( -1 ), pc >> 8 );
		sp = SP( -2 );
		WRITE_STACK( sp, pc );
		goto branch_taken;
	
	case 0x20: { // JSR
		int temp = pc + 1;
		pc = GET_ADDR();
		WRITE_STACK( SP( -1 ), temp >> 8 );
		sp = SP( -2 );
		WRITE_STACK( sp, temp );
		goto loop;
	}
	
	case 0x60: // RTS
		pc = 1 + READ_STACK( sp );
		pc += 0x100 * READ_STACK( SP( 1 ) );
		sp = SP( 2 );
		goto loop;
	
	case 0x00: // BRK
		goto handle_brk;
	
// Common

	case 0xBD:{// LDA abs,X
		PAGE_PENALTY( data + x );
		int addr = GET_ADDR() + x;
		pc += 2;
		READ_FAST( addr, nz );
		a = nz;
		goto loop;
	}
	
	case 0x9D:{// STA abs,X
		int addr = GET_ADDR() + x;
		pc += 2;
		WRITE_FAST( addr, a );
		goto loop;
	}
	
	case 0x95: // STA zp,x
		data = BYTE( data + x );
	case 0x85: // STA zp
		pc++;
		WRITE_LOW( data, a );
		goto loop;
	
	case 0xAE:{// LDX abs
		int addr = GET_ADDR();
		pc += 2;
		READ_FAST( addr, nz );
		x = nz;
		goto loop;
	}
	
	case 0xA5: // LDA zp
		a = nz = READ_LOW( data );
		pc++;
		goto loop;
	
// Load/store
	
	{
		int addr;
	case 0x91: // STA (ind),Y
		addr = 0x100 * READ_LOW( BYTE( data + 1 ) );
		addr += READ_LOW( data ) + y;
		pc++;
		goto sta_ptr;
	
	case 0x81: // STA (ind,X)
		data = BYTE( data + x );
	case 0x92: // STA (ind)
		addr = 0x100 * READ_LOW( BYTE( data + 1 ) );
		addr += READ_LOW( data );
		pc++;
		goto sta_ptr;
	
	case 0x99: // STA abs,Y
		data += y;
	case 0x8D: // STA abs
		addr = data + 0x100 * GET_MSB();
		pc += 2;
	sta_ptr:
		WRITE_FAST( addr, a );
		goto loop;
	}
	
	{
		int addr;
	case 0xA1: // LDA (ind,X)
		data = BYTE( data + x );
	case 0xB2: // LDA (ind)
		addr = 0x100 * READ_LOW( BYTE( data + 1 ) );
		addr += READ_LOW( data );
		pc++;
		goto a_nz_read_addr;
	
	case 0xB1:// LDA (ind),Y
		addr = READ_LOW( data ) + y;
		PAGE_PENALTY( addr );
		addr += 0x100 * READ_LOW( BYTE( data + 1 ) );
		pc++;
		goto a_nz_read_addr;
	
	case 0xB9: // LDA abs,Y
		data += y;
		PAGE_PENALTY( data );
	case 0xAD: // LDA abs
		addr = data + 0x100 * GET_MSB();
		pc += 2;
	a_nz_read_addr:
		READ_FAST( addr, nz );
		a = nz;
		goto loop;
	}

	case 0xBE:{// LDX abs,y
		PAGE_PENALTY( data + y );
		int addr = GET_ADDR() + y;
		pc += 2;
		FLUSH_TIME();
		x = nz = READ_MEM( addr );
		CACHE_TIME();
		goto loop;
	}
	
	case 0xB5: // LDA zp,x
		a = nz = READ_LOW( BYTE( data + x ) );
		pc++;
		goto loop;
	
	case 0xA9: // LDA #imm
		pc++;
		a  = data;
		nz = data;
		goto loop;

// Bit operations

	case 0x3C: // BIT abs,x
		data += x;
	case 0x2C:{// BIT abs
		int addr;
		ADD_PAGE( addr );
		FLUSH_TIME();
		nz = READ_MEM( addr );
		CACHE_TIME();
		goto bit_common;
	}
	case 0x34: // BIT zp,x
		data = BYTE( data + x );
	case 0x24: // BIT zp
		data = READ_LOW( data );
	case 0x89: // BIT imm
		nz = data;
	bit_common:
		pc++;
		flags = (flags & ~v40) + (nz & v40);
		if ( nz & a )
			goto loop; // Z should be clear, and nz must be non-zero if nz & a is
		nz <<= 8; // set Z flag without affecting N flag
		goto loop;
		
	{
		int addr;
		
	case 0xB3: // TST abs,x
		addr = GET_MSB() + x;
		goto tst_abs;
	
	case 0x93: // TST abs
		addr = GET_MSB();
	tst_abs:
		addr += 0x100 * instr [2];
		pc++;
		FLUSH_TIME();
		nz = READ_MEM( addr );
		CACHE_TIME();
		goto tst_common;
	}
	
	case 0xA3: // TST zp,x
		nz = READ_LOW( BYTE( GET_MSB() + x ) );
		goto tst_common;
	
	case 0x83: // TST zp
		nz = READ_LOW( GET_MSB() );
	tst_common:
		pc += 2;
		flags = (flags & ~v40) + (nz & v40);
		if ( nz & data )
			goto loop; // Z should be clear, and nz must be non-zero if nz & data is
		nz <<= 8; // set Z flag without affecting N flag
		goto loop;
	
	{
		int addr;
	case 0x0C: // TSB abs
	case 0x1C: // TRB abs
		addr = GET_ADDR();
		pc++;
		goto txb_addr;
	
	// TODO: everyone lists different behaviors for the flags flags, ugh
	case 0x04: // TSB zp
	case 0x14: // TRB zp
		addr = data + ram_addr;
	txb_addr:
		FLUSH_TIME();
		nz = a | READ_MEM( addr );
		if ( opcode & 0x10 )
			nz ^= a; // bits from a will already be set, so this clears them
		flags = (flags & ~v40) + (nz & v40);
		pc++;
		WRITE_MEM( addr, nz );
		CACHE_TIME();
		goto loop;
	}
	
	case 0x07: // RMBn
	case 0x17:
	case 0x27:
	case 0x37:
	case 0x47:
	case 0x57:
	case 0x67:
	case 0x77:
		pc++;
		READ_LOW( data ) &= ~(1 << (opcode >> 4));
		goto loop;
	
	case 0x87: // SMBn
	case 0x97:
	case 0xA7:
	case 0xB7:
	case 0xC7:
	case 0xD7:
	case 0xE7:
	case 0xF7:
		pc++;
		READ_LOW( data ) |= 1 << ((opcode >> 4) - 8);
		goto loop;
	
// Load/store
	
	case 0x9E: // STZ abs,x
		data += x;
	case 0x9C: // STZ abs
		ADD_PAGE( data );
		pc++;
		FLUSH_TIME();
		WRITE_MEM( data, 0 );
		CACHE_TIME();
		goto loop;
	
	case 0x74: // STZ zp,x
		data = BYTE( data + x );
	case 0x64: // STZ zp
		pc++;
		WRITE_LOW( data, 0 );
		goto loop;
	
	case 0x94: // STY zp,x
		data = BYTE( data + x );
	case 0x84: // STY zp
		pc++;
		WRITE_LOW( data, y );
		goto loop;
	
	case 0x96: // STX zp,y
		data = BYTE( data + y );
	case 0x86: // STX zp
		pc++;
		WRITE_LOW( data, x );
		goto loop;
	
	case 0xB6: // LDX zp,y
		data = BYTE( data + y );
	case 0xA6: // LDX zp
		data = READ_LOW( data );
	case 0xA2: // LDX #imm
		pc++;
		x = data;
		nz = data;
		goto loop;
	
	case 0xB4: // LDY zp,x
		data = BYTE( data + x );
	case 0xA4: // LDY zp
		data = READ_LOW( data );
	case 0xA0: // LDY #imm
		pc++;
		y = data;
		nz = data;
		goto loop;
	
	case 0xBC: // LDY abs,X
		data += x;
		PAGE_PENALTY( data );
	case 0xAC:{// LDY abs
		int addr = data + 0x100 * GET_MSB();
		pc += 2;
		FLUSH_TIME();
		y = nz = READ_MEM( addr );
		CACHE_TIME();
		goto loop;
	}
	
	{
		int temp;
	case 0x8C: // STY abs
		temp = y;
		if ( 0 )
	case 0x8E: // STX abs
			temp = x;
		int addr = GET_ADDR();
		pc += 2;
		FLUSH_TIME();
		WRITE_MEM( addr, temp );
		CACHE_TIME();
		goto loop;
	}

// Compare

	case 0xEC:{// CPX abs
		int addr = GET_ADDR();
		pc++;
		FLUSH_TIME();
		data = READ_MEM( addr );
		CACHE_TIME();
		goto cpx_data;
	}
	
	case 0xE4: // CPX zp
		data = READ_LOW( data );
	case 0xE0: // CPX #imm
	cpx_data:
		nz = x - data;
		pc++;
		c = ~nz;
		nz = BYTE( nz );
		goto loop;
	
	case 0xCC:{// CPY abs
		int addr = GET_ADDR();
		pc++;
		FLUSH_TIME();
		data = READ_MEM( addr );
		CACHE_TIME();
		goto cpy_data;
	}
	
	case 0xC4: // CPY zp
		data = READ_LOW( data );
	case 0xC0: // CPY #imm
	cpy_data:
		nz = y - data;
		pc++;
		c = ~nz;
		nz = BYTE( nz );
		goto loop;
	
// Logical

#define ARITH_ADDR_MODES( op )\
	case op - 0x04: /* (ind,x) */\
		data = BYTE( data + x );\
	case op + 0x0D: /* (ind) */\
		data = 0x100 * READ_LOW( BYTE( data + 1 ) ) + READ_LOW( data );\
		goto ptr##op;\
	case op + 0x0C:{/* (ind),y */\
		int temp = READ_LOW( data ) + y;\
		PAGE_PENALTY( temp );\
		data = temp + 0x100 * READ_LOW( BYTE( data + 1 ) );\
		goto ptr##op;\
	}\
	case op + 0x10: /* zp,X */\
		data = BYTE( data + x );\
	case op + 0x00: /* zp */\
		data = READ_LOW( data );\
		goto imm##op;\
	case op + 0x14: /* abs,Y */\
		data += y;\
		goto ind##op;\
	case op + 0x18: /* abs,X */\
		data += x;\
	ind##op:\
		PAGE_PENALTY( data );\
	case op + 0x08: /* abs */\
		ADD_PAGE( data );\
	ptr##op:\
		FLUSH_TIME();\
		data = READ_MEM( data );\
		CACHE_TIME();\
	case op + 0x04: /* imm */\
	imm##op:

	ARITH_ADDR_MODES( 0xC5 ) // CMP
		nz = a - data;
		pc++;
		c = ~nz;
		nz = BYTE( nz );
		goto loop;
	
	ARITH_ADDR_MODES( 0x25 ) // AND
		nz = (a &= data);
		pc++;
		goto loop;
	
	ARITH_ADDR_MODES( 0x45 ) // EOR
		nz = (a ^= data);
		pc++;
		goto loop;
	
	ARITH_ADDR_MODES( 0x05 ) // ORA
		nz = (a |= data);
		pc++;
		goto loop;
	
// Add/subtract

	ARITH_ADDR_MODES( 0xE5 ) // SBC
		data ^= 0xFF;
		goto adc_imm;
	
	ARITH_ADDR_MODES( 0x65 ) // ADC
	adc_imm: {
		if ( flags & d08 )
			dprintf( "Decimal mode not supported\n" );
		int carry = c >> 8 & 1;
		int ov = (a ^ 0x80) + carry + SBYTE( data );
		flags = (flags & ~v40) + (ov >> 2 & v40);
		c = nz = a + data + carry;
		pc++;
		a = BYTE( nz );
		goto loop;
	}
	
// Shift/rotate

	case 0x4A: // LSR A
		c = 0;
	case 0x6A: // ROR A
		nz = c >> 1 & 0x80;
		c = a << 8;
		nz += a >> 1;
		a = nz;
		goto loop;

	case 0x0A: // ASL A
		nz = a << 1;
		c = nz;
		a = BYTE( nz );
		goto loop;

	case 0x2A: { // ROL A
		nz = a << 1;
		int temp = c >> 8 & 1;
		c = nz;
		nz += temp;
		a = BYTE( nz );
		goto loop;
	}
	
	case 0x5E: // LSR abs,X
		data += x;
	case 0x4E: // LSR abs
		c = 0;
	case 0x6E: // ROR abs
	ror_abs: {
		ADD_PAGE( data );
		FLUSH_TIME();
		int temp = READ_MEM( data );
		nz = (c >> 1 & 0x80) + (temp >> 1);
		c = temp << 8;
		goto rotate_common;
	}
	
	case 0x3E: // ROL abs,X
		data += x;
		goto rol_abs;
	
	case 0x1E: // ASL abs,X
		data += x;
	case 0x0E: // ASL abs
		c = 0;
	case 0x2E: // ROL abs
	rol_abs:
		ADD_PAGE( data );
		nz = c >> 8 & 1;
		FLUSH_TIME();
		nz += (c = READ_MEM( data ) << 1);
	rotate_common:
		pc++;
		WRITE_MEM( data, BYTE( nz ) );
		CACHE_TIME();
		goto loop;
	
	case 0x7E: // ROR abs,X
		data += x;
		goto ror_abs;
	
	case 0x76: // ROR zp,x
		data = BYTE( data + x );
		goto ror_zp;
	
	case 0x56: // LSR zp,x
		data = BYTE( data + x );
	case 0x46: // LSR zp
		c = 0;
	case 0x66: // ROR zp
	ror_zp: {
		int temp = READ_LOW( data );
		nz = (c >> 1 & 0x80) + (temp >> 1);
		c = temp << 8;
		goto write_nz_zp;
	}
	
	case 0x36: // ROL zp,x
		data = BYTE( data + x );
		goto rol_zp;
	
	case 0x16: // ASL zp,x
		data = BYTE( data + x );
	case 0x06: // ASL zp
		c = 0;
	case 0x26: // ROL zp
	rol_zp:
		nz = c >> 8 & 1;
		nz += (c = READ_LOW( data ) << 1);
		goto write_nz_zp;
	
// Increment/decrement

#define INC_DEC( reg, n ) reg = BYTE( nz = reg + n ); goto loop;

	case 0x1A: // INA
		INC_DEC( a, +1 )
	
	case 0xE8: // INX
		INC_DEC( x, +1 )
	
	case 0xC8: // INY
		INC_DEC( y, +1 )

	case 0x3A: // DEA
		INC_DEC( a, -1 )
	
	case 0xCA: // DEX
		INC_DEC( x, -1 )
	
	case 0x88: // DEY
		INC_DEC( y, -1 )
	
	case 0xF6: // INC zp,x
		data = BYTE( data + x );
	case 0xE6: // INC zp
		nz = 1;
		goto add_nz_zp;
	
	case 0xD6: // DEC zp,x
		data = BYTE( data + x );
	case 0xC6: // DEC zp
		nz = -1;
	add_nz_zp:
		nz += READ_LOW( data );
	write_nz_zp:
		pc++;
		WRITE_LOW( data, nz );
		goto loop;
	
	case 0xFE: // INC abs,x
		data = x + GET_ADDR();
		goto inc_ptr;
	
	case 0xEE: // INC abs
		data = GET_ADDR();
	inc_ptr:
		nz = 1;
		goto inc_common;
	
	case 0xDE: // DEC abs,x
		data = x + GET_ADDR();
		goto dec_ptr;
	
	case 0xCE: // DEC abs
		data = GET_ADDR();
	dec_ptr:
		nz = -1;
	inc_common:
		FLUSH_TIME();
		pc += 2;
		nz += READ_MEM( data );
		WRITE_MEM( data, BYTE( nz ) );
		CACHE_TIME();
		goto loop;
		
// Transfer

	case 0xA8: // TAY
		y = nz = a;
		goto loop;
	
	case 0x98: // TYA
		a = nz = y;
		goto loop;
	
	case 0xAA: // TAX
		x = nz = a;
		goto loop;
		
	case 0x8A: // TXA
		a = nz = x;
		goto loop;

	case 0x9A: // TXS
		SET_SP( x ); // verified (no flag change)
		goto loop;
	
	case 0xBA: // TSX
		x = nz = GET_SP();
		goto loop;
	
	#define SWAP_REGS( r1, r2 ) {\
		int t = r1;\
		r1 = r2;\
		r2 = t;\
		goto loop;\
	}
	
	case 0x02: // SXY
		SWAP_REGS( x, y );
	
	case 0x22: // SAX
		SWAP_REGS( a, x );
	
	case 0x42: // SAY
		SWAP_REGS( a, y );
	
	case 0x62: // CLA
		a = 0;
		goto loop;
	
	case 0x82: // CLX
		x = 0;
		goto loop;
	
	case 0xC2: // CLY
		y = 0;
		goto loop;
	
// Stack
	
	case 0x48: // PHA
		sp = SP( -1 );
		WRITE_STACK( sp, a );
		goto loop;
		
	case 0x68: // PLA
		a = nz = READ_STACK( sp );
		sp = SP( 1 );
		goto loop;
	
	case 0xDA: // PHX
		sp = SP( -1 );
		WRITE_STACK( sp, x );
		goto loop;
		
	case 0x5A: // PHY
		sp = SP( -1 );
		WRITE_STACK( sp, y );
		goto loop;
		
	case 0x40:{// RTI
		pc  = READ_STACK( SP( 1 ) );
		pc += READ_STACK( SP( 2 ) ) * 0x100;
		int temp = READ_STACK( sp );
		sp = SP( 3 );
		data = flags;
		SET_FLAGS( temp );
		CPU.r.flags = flags; // update externally-visible I flag
		if ( (data ^ flags) & i04 )
		{
			time_t new_time = CPU.end_time_;
			if ( !(flags & i04) && new_time > CPU.irq_time_ )
				new_time = CPU.irq_time_;
			int delta = s.base - new_time;
			s.base = new_time;
			s_time += delta;
		}
		goto loop;
	}
	
	case 0xFA: // PLX
		x = nz = READ_STACK( sp );
		sp = SP( 1 );
		goto loop;
	
	case 0x7A: // PLY
		y = nz = READ_STACK( sp );
		sp = SP( 1 );
		goto loop;
	
	case 0x28:{// PLP
		int temp = READ_STACK( sp );
		sp = SP( 1 );
		int changed = flags ^ temp;
		SET_FLAGS( temp );
		if ( !(changed & i04) )
			goto loop; // I flag didn't change
		if ( flags & i04 )
			goto handle_sei;
		goto handle_cli;
	}
	
	case 0x08:{// PHP
		int temp;
		GET_FLAGS( temp );
		sp = SP( -1 );
		WRITE_STACK( sp, temp | b10 );
		goto loop;
	}
	
// Flags

	case 0x38: // SEC
		c = 0x100;
		goto loop;
	
	case 0x18: // CLC
		c = 0;
		goto loop;
		
	case 0xB8: // CLV
		flags &= ~v40;
		goto loop;
	
	case 0xD8: // CLD
		flags &= ~d08;
		goto loop;
	
	case 0xF8: // SED
		flags |= d08;
		goto loop;
	
	case 0x58: // CLI
		if ( !(flags & i04) )
			goto loop;
		flags &= ~i04;
	handle_cli: {
		//dprintf( "CLI at %d\n", TIME );
		CPU.r.flags = flags; // update externally-visible I flag
		int delta = s.base - CPU.irq_time_;
		if ( delta <= 0 )
		{
			if ( TIME() < CPU.irq_time_ )
				goto loop;
			goto delayed_cli;
		}
		s.base = CPU.irq_time_;
		s_time += delta;
		if ( s_time < 0 )
			goto loop;
		
		if ( delta >= s_time + 1 )
		{
			// delayed irq until after next instruction
			s.base += s_time + 1;
			s_time = -1;
			CPU.irq_time_ = s.base; // TODO: remove, as only to satisfy debug check in loop
			goto loop;
		}
		
		// TODO: implement
	delayed_cli:
		dprintf( "Delayed CLI not supported\n" );
		goto loop;
	}
	
	case 0x78: // SEI
		if ( flags & i04 )
			goto loop;
		flags |= i04;
	handle_sei: {
		CPU.r.flags = flags; // update externally-visible I flag
		int delta = s.base - CPU.end_time_;
		s.base = CPU.end_time_;
		s_time += delta;
		if ( s_time < 0 )
			goto loop;
		
		dprintf( "Delayed SEI not supported\n" );
		goto loop;
	}
	
// Special
	
	case 0x53:{// TAM
		int bits = data; // avoid using data across function call
		pc++;
		for ( int i = 0; i < 8; i++ )
			if ( bits & (1 << i) )
				SET_MMR( i, a );
		goto loop;
	}
	
	case 0x43:{// TMA
		pc++;
		byte const* in = CPU.mmr;
		do
		{
			if ( data & 1 )
				a = *in;
			in++;
		}
		while ( (data >>= 1) != 0 );
		goto loop;
	}
	
	case 0x03: // ST0
	case 0x13: // ST1
	case 0x23:{// ST2
		int addr = opcode >> 4;
		if ( addr )
			addr++;
		pc++;
		FLUSH_TIME();
		WRITE_VDP( addr, data );
		CACHE_TIME();
		goto loop;
	}
	
	case 0xEA: // NOP
		goto loop;

	case 0x54: // CSL
		dprintf( "CSL not supported\n" );
		illegal_encountered = true;
		goto loop;
	
	case 0xD4: // CSH
		goto loop;
	
	case 0xF4: { // SET
		//int operand = GET_MSB();
		dprintf( "SET not handled\n" );
		//switch ( data )
		//{
		//}
		illegal_encountered = true;
		goto loop;
	}
	
// Block transfer

	{
		int in_alt;
		int in_inc;
		int out_alt;
		int out_inc;
		
	case 0xE3: // TIA
		in_alt  = 0;
		goto bxfer_alt;
	
	case 0xF3: // TAI
		in_alt  = 1;
	bxfer_alt:
		in_inc  = in_alt ^ 1;
		out_alt = in_inc;
		out_inc = in_alt;
		goto bxfer;
	
	case 0xD3: // TIN
		in_inc  = 1;
		out_inc = 0;
		goto bxfer_no_alt;
	
	case 0xC3: // TDD
		in_inc  = -1;
		out_inc = -1;
		goto bxfer_no_alt;
	
	case 0x73: // TII
		in_inc  = 1;
		out_inc = 1;
	bxfer_no_alt:
		in_alt  = 0;
		out_alt = 0;
	bxfer:
		int in    = GET_LE16( instr + 0 );
		int out   = GET_LE16( instr + 2 );
		int count = GET_LE16( instr + 4 );
		if ( !count )
			count = 0x10000;
		pc += 6;
		WRITE_STACK( SP( -1 ), y );
		WRITE_STACK( SP( -2 ), a );
		WRITE_STACK( SP( -3 ), x );
		FLUSH_TIME();
		do
		{
			// TODO: reads from $0800-$1400 in I/O page should do I/O
			int t = READ_MEM( in );
			in = WORD( in + in_inc );
			s.time += 6;
			if ( in_alt )
				in_inc = -in_inc;
			WRITE_MEM( out, t );
			out = WORD( out + out_inc );
			if ( out_alt )
				out_inc = -out_inc;
		}
		while ( --count );
		CACHE_TIME();
		goto loop;
	}

// Illegal

	default:
		check( (unsigned) opcode <= 0xFF );
		dprintf( "Illegal opcode $%02X at $%04X\n", (int) opcode, (int) pc - 1 );
		illegal_encountered = true;
		goto loop;
	}
	assert( false ); // catch missing 'goto loop' or accidental 'break'
	
	int result_;
handle_brk:
	pc++;
	result_ = 6;
	
interrupt:
	{
		s_time += 7;
		
		// Save PC and read vector
		WRITE_STACK( SP( -1 ), pc >> 8 );
		WRITE_STACK( SP( -2 ), pc );
		pc = GET_LE16( &READ_CODE( 0xFFF0 ) + result_ );
		
		// Save flags
		int temp;
		GET_FLAGS( temp );
		if ( result_ == 6 )
			temp |= b10; // BRK sets B bit
		sp = SP( -3 );
		WRITE_STACK( sp, temp );
		
		// Update I flag in externally-visible flags
		flags &= ~d08;
		CPU.r.flags = (flags |= i04);
		
		// Update time
		int delta = s.base - CPU.end_time_;
		if ( delta >= 0 )
			goto loop;
		s_time += delta;
		s.base = CPU.end_time_;
		goto loop;
	}
	
idle_done:
	s_time = 0;
	
out_of_time:
	pc--;
	
	// Optional action that triggers interrupt or changes irq/end time
	#ifdef CPU_DONE
	{
		CPU_DONE( result_ );
		if ( result_ >= 0 )
			goto interrupt;
		if ( s_time < 0 )
			goto loop;
	}
	#endif
	
	// Flush cached state
	CPU.r.pc = pc;
	CPU.r.sp = GET_SP();
	CPU.r.a  = a;
	CPU.r.x  = x;
	CPU.r.y  = y;
	
	int temp;
	GET_FLAGS( temp );
	CPU.r.flags = temp;
	
	CPU.cpu_state_.base = s.base;
	CPU.cpu_state_.time = s_time;
	CPU.cpu_state = &CPU.cpu_state_;
}
