// $package. http://www.slack.net/~ant/

// Last validated with zexall 2009.12.05.
// Doesn't implement the R register or immediate interrupt after EI.
// Address wrap-around isn't completely correct, but is prevented from crashing emulator.
// 16-bit memory accesses are made directly to mapped memory, instead of using macro.

#if 0
/* Define these macros in the source file before #including this file.
- Parameters might be expressions, so they are best evaluated only once,
though they NEVER have side-effects, so multiple evaluation is OK.
- Output parameters might be a multiple-assignment expression like "a=x",
so they must NOT be parenthesized.
- Except where noted, time() and related functions will NOT work
correctly inside a macro. TIME() is always correct, and between FLUSH_TIME() and
CACHE_TIME() the normal time changing functions can be used.
- Macros "returning" void may use a {} statement block. */

    // 0 <= addr <= 0xFFFF + 0x100
    // Optional; default uses whatever was set with map_mem()
    int  READ_CODE( addr_t );

    // 0 <= addr <= 0xFFFF + 0x100
	// Optional; default uses whatever was set with map_mem()
	int  READ_MEM(  addr_t );
	void WRITE_MEM( addr_t, int data );
	
	// 0 <= port <= 0xFFFF (apparently upper 8 bits are output by hardware)
	void OUT_PORT( int port, int data );
	int  IN_PORT   int port );

	// Reference to Z80_Cpu object used for emulation
	#define CPU cpu
	
// The following can be used within macros:
	
	// Current time
	time_t TIME();
	
	// Allows use of time functions
	void FLUSH_TIME();
	
	// Must be used before end of macro if FLUSH_TIME() was used earlier
	void CACHE_TIME();

// Configuration (optional; commented behavior if defined)
	
	// Optimizes as if map_mem( 0, 0x10000, FLAT_MEM, FLAT_MEM ) is always in effect
    #define FLAT_MEM my_mem_array
	
	// If RST 7 ($FF) is encountered and PC = IDLE_ADDR, stops execution
	#define IDLE_ADDR 0x1234
	
	// Expanded just before beginning of code, to help debugger
	#define CPU_BEGIN void my_run_cpu() {
	
#endif

/* Copyright (C) 2006-2008 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#ifdef CPU_BEGIN
	CPU_BEGIN
#endif

#define R CPU.r

// flags, named with hex value for clarity
int const S80 = 0x80;
int const Z40 = 0x40;
int const F20 = 0x20;
int const H10 = 0x10;
int const F08 = 0x08;
int const V04 = 0x04;
int const P04 = 0x04;
int const N02 = 0x02;
int const C01 = 0x01;

#define SZ28P( n )  CPU.szpc [n]
#define SZ28PC( n ) CPU.szpc [n]
#define SZ28C( n )  (CPU.szpc [n] & ~P04)
#define SZ28( n )   SZ28C( n )

#define SET_R( n )  (void) (R.r = n)
#define GET_R()     (R.r)

// Time
#define TIME()          (s_time + s.base)
#define FLUSH_TIME()    {s.time = s_time;}
#define CACHE_TIME()    {s_time = s.time;}

// Memory
#define RW_MEM( addr, rw )          RW_PAGE( addr, rw ) [RW_OFFSET( addr )]
#ifndef READ_CODE
    #define READ_CODE( addr )       RW_MEM( addr, read )
#endif

#ifdef FLAT_MEM
	#define RW_PAGE( addr, rw )     FLAT_MEM
	#define RW_OFFSET( addr )       (addr)
	#define INSTR( off, addr )      READ_CODE( addr )
#else
	#define RW_PAGE( addr, rw )     s.rw [(unsigned) (addr) >> Z80_Cpu::page_bits]
	#define RW_OFFSET( addr )       Z80_CPU_OFFSET( addr )
	#define INSTR( off, addr )      instr [off]
#endif

#ifndef READ_MEM
	#define READ_MEM( addr )        RW_MEM( addr, read )
#endif
	
#ifndef WRITE_MEM
	#define WRITE_MEM( addr, data ) (RW_MEM( addr, write ) = data)
#endif

#define READ_WORD( addr )           GET_LE16( &RW_MEM( addr, read ) )
#define WRITE_WORD( addr, data )    SET_LE16( &RW_MEM( addr, write ), data )

// Truncation
#define BYTE(  n ) ((BOOST::uint8_t ) (n)) /* (unsigned) n & 0xFF */
#define SBYTE( n ) ((BOOST::int8_t  ) (n)) /* (BYTE( n ) ^ 0x80) - 0x80 */
#define WORD(  n ) ((BOOST::uint16_t) (n)) /* (unsigned) n & 0xFFFF */

// Misc
#define CASE5( a, b, c, d, e          ) case 0x##a:case 0x##b:case 0x##c:case 0x##d:case 0x##e
#define CASE6( a, b, c, d, e, f       ) CASE5( a, b, c, d, e       ): case 0x##f
#define CASE7( a, b, c, d, e, f, g    ) CASE6( a, b, c, d, e, f    ): case 0x##g
#define CASE8( a, b, c, d, e, f, g, h ) CASE7( a, b, c, d, e, f, g ): case 0x##h

#if BLARGG_BIG_ENDIAN
	#define R8( n, offset ) ((r.r8_ - offset) [n]) 
#elif BLARGG_LITTLE_ENDIAN
	#define R8( n, offset ) ((r.r8_ - offset) [(n) ^ 1]) 
#else
	#error "Byte order of CPU must be known"
#endif

#define R16( n, shift, offset ) (r.r16_ [((unsigned) (n) >> shift) - (offset >> shift)])

#define EX( x, y ) \
	{\
		int temp = x;\
		x = y;\
		y = temp;\
	}
		
#define EXX( name ) \
	EX( R.alt.name, r.name )
		
bool warning = false;
{
	Z80_Cpu::cpu_state_t s;
	#ifdef FLAT_MEM
		s.base = CPU.cpu_state_.base;
	#else
		s = CPU.cpu_state_;
	#endif
	CPU.cpu_state = &s;
	
	
	union r_t {
		Z80_Cpu::regs_t b;
		Z80_Cpu::pairs_t w;
		byte r8_ [8]; // indexed
		BOOST::uint16_t r16_ [4];
	} r;
	r.b = R.b;
	
	Z80_Cpu::time_t s_time = CPU.cpu_state_.time;
	int pc = R.pc;
	int sp = R.sp;
	int ix = R.ix; // TODO: keep in memory for direct access?
	int iy = R.iy;
	int flags = R.b.flags;
	
	//goto loop; // confuses optimizer
	s_time += 7;
	pc -= 2;
	
call_not_taken:
	s_time -= 7; 
jp_not_taken:
	pc += 2;
loop:
	
	check( (unsigned) pc < 0x10000 + 1 ); // +1 so emulator can catch wrap-around
	check( (unsigned) sp < 0x10000 );
	check( (unsigned) flags < 0x100 );
	check( (unsigned) ix < 0x10000 );
	check( (unsigned) iy < 0x10000 );
	
    byte const* instr = RW_PAGE( pc, read );

    int opcode;
	
    if ( RW_OFFSET( ~0 ) == ~0 )
    {
        opcode = instr [RW_OFFSET( pc )];
        pc++;
        instr += RW_OFFSET( pc );
    }
    else
    {
        instr += RW_OFFSET( pc );
        opcode = *instr++;
        pc++;
    }

    static byte const clock_table [256 * 2] = {
	//   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		 4,10, 7, 6, 4, 4, 7, 4, 4,11, 7, 6, 4, 4, 7, 4, // 0
		 8,10, 7, 6, 4, 4, 7, 4,12,11, 7, 6, 4, 4, 7, 4, // 1
		 7,10,16, 6, 4, 4, 7, 4, 7,11,16, 6, 4, 4, 7, 4, // 2
		 7,10,13, 6,11,11,10, 4, 7,11,13, 6, 4, 4, 7, 4, // 3
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 4
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 5
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 6
		 7, 7, 7, 7, 7, 7, 4, 7, 4, 4, 4, 4, 4, 4, 7, 4, // 7
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 8
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // 9
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // A
		 4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, // B
		11,10,10,10,17,11, 7,11,11,10,10, 8,17,17, 7,11, // C
		11,10,10,11,17,11, 7,11,11, 4,10,11,17, 8, 7,11, // D
		11,10,10,19,17,11, 7,11,11, 4,10, 4,17, 8, 7,11, // E
		11,10,10, 4,17,11, 7,11,11, 6,10, 4,17, 8, 7,11, // F
		
		// high four bits are $ED time - 8, low four bits are $DD/$FD time - 8
		//0    1    2    3    4    5    6    7    8    9    A    B    C    D    E    F
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x06,0x0C,0x02,0x00,0x00,0x03,0x00,0x00,0x07,0x0C,0x02,0x00,0x00,0x03,0x00,
		0x00,0x00,0x00,0x00,0x0F,0x0F,0x0B,0x00,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,
		0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0x10,0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0x10,
		0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0x10,0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0x10,
		0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0xA0,0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0xA0,
		0x4B,0x4B,0x7B,0xCB,0x0B,0x6B,0x00,0x0B,0x40,0x40,0x70,0xC0,0x00,0x60,0x0B,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0B,0x00,
		0x80,0x80,0x80,0x80,0x00,0x00,0x0B,0x00,0x80,0x80,0x80,0x80,0x00,0x00,0x0B,0x00,
		0xD0,0xD0,0xD0,0xD0,0x00,0x00,0x0B,0x00,0xD0,0xD0,0xD0,0xD0,0x00,0x00,0x0B,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0F,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x06,0x00,0x0F,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x00,
	};
	
	if ( s_time >= 0 )
		goto out_of_time;
	s_time += clock_table [opcode];
	
	#ifdef Z80_CPU_LOG_H
		//log_opcode( opcode, READ_CODE( pc ) );
		z80_cpu_log( "log.txt", pc - 1, opcode, READ_CODE( pc ),
				READ_CODE( pc + 1 ), READ_CODE( pc + 2 ) );
		z80_log_regs( r.b.a, r.w.bc, r.w.de, r.w.hl, sp, ix, iy );
	#endif
	
#define GET_ADDR()  GET_LE16( &INSTR( 0, pc ) )

	int data;
    data = INSTR( 0, pc );
	
	switch ( opcode )
	{
// Common

	case 0x00: // NOP
	CASE7( 40, 49, 52, 5B, 64, 6D, 7F ): // LD B,B etc.
		goto loop;
	
	case 0x08:{// EX AF,AF'
		EXX( b.a );
		EX( R.alt.b.flags, flags );
		goto loop;
	}
	
	case 0xD3: // OUT (imm),A
		pc++;
		OUT_PORT( (data + r.b.a * 0x100), r.b.a );
		goto loop;
		
	case 0x2E: // LD L,imm
		pc++;
		r.b.l = data;
		goto loop;
	
	case 0x3E: // LD A,imm
		pc++;
		r.b.a = data;
		goto loop;
	
	case 0x3A:{// LD A,(addr)
		int addr = GET_ADDR();
		pc += 2;
		r.b.a = READ_MEM( addr );
		goto loop;
	}
	
// Conditional

#define ZERO    (flags & Z40)
#define CARRY   (flags & C01)
#define EVEN    (flags & P04)
#define MINUS   (flags & S80)

// JR
// TODO: more efficient way to handle negative branch that wraps PC around
#define JR_( cond, clocks ) {\
	pc++;\
	if ( !(cond) )\
		goto loop;\
	int offset = SBYTE( data );\
	pc = WORD( pc + offset );\
	s_time += clocks;\
	goto loop;\
}

#define JR( cond ) JR_( cond, 5 )
	
	case 0x20: JR( !ZERO  ) // JR NZ,disp
	case 0x28: JR(  ZERO  ) // JR Z,disp
	case 0x30: JR( !CARRY ) // JR NC,disp
	case 0x38: JR(  CARRY ) // JR C,disp
	case 0x18: JR_( true,0) // JR disp

	case 0x10:{// DJNZ disp
		int temp = r.b.b - 1;
		r.b.b = temp;
		JR( temp )
	}
	
// JP
#define JP( cond ) \
	if ( !(cond) )\
		goto jp_not_taken;\
	pc = GET_ADDR();\
	goto loop;
	
	case 0xC2: JP( !ZERO  ) // JP NZ,addr
	case 0xCA: JP(  ZERO  ) // JP Z,addr
	case 0xD2: JP( !CARRY ) // JP NC,addr
	case 0xDA: JP(  CARRY ) // JP C,addr
	case 0xE2: JP( !EVEN  ) // JP PO,addr
	case 0xEA: JP(  EVEN  ) // JP PE,addr
	case 0xF2: JP( !MINUS ) // JP P,addr
	case 0xFA: JP(  MINUS ) // JP M,addr
	
	case 0xC3: // JP addr
		pc = GET_ADDR();
		goto loop;
	
	case 0xE9: // JP HL
		pc = r.w.hl;
		goto loop;

// RET
#define RET( cond ) \
	if ( cond )\
		goto ret_taken;\
	s_time -= 6;\
	goto loop;
	
	case 0xC0: RET( !ZERO  ) // RET NZ
	case 0xC8: RET(  ZERO  ) // RET Z
	case 0xD0: RET( !CARRY ) // RET NC
	case 0xD8: RET(  CARRY ) // RET C
	case 0xE0: RET( !EVEN  ) // RET PO
	case 0xE8: RET(  EVEN  ) // RET PE
	case 0xF0: RET( !MINUS ) // RET P
	case 0xF8: RET(  MINUS ) // RET M
	
	case 0xC9: // RET
	ret_taken:
		pc = READ_WORD( sp );
		sp = WORD( sp + 2 );
		goto loop;
	
// CALL
#define CALL( cond ) \
	if ( cond )\
		goto call_taken;\
	goto call_not_taken;

	case 0xC4: CALL( !ZERO  ) // CALL NZ,addr
	case 0xCC: CALL(  ZERO  ) // CALL Z,addr
	case 0xD4: CALL( !CARRY ) // CALL NC,addr
	case 0xDC: CALL(  CARRY ) // CALL C,addr
	case 0xE4: CALL( !EVEN  ) // CALL PO,addr
	case 0xEC: CALL(  EVEN  ) // CALL PE,addr
	case 0xF4: CALL( !MINUS ) // CALL P,addr
	case 0xFC: CALL(  MINUS ) // CALL M,addr
	
	case 0xCD:{// CALL addr
	call_taken:
		int addr = pc + 2;
		pc = GET_ADDR();
		sp = WORD( sp - 2 );
		WRITE_WORD( sp, addr );
		goto loop;
	}
	
	case 0xFF: // RST
		#ifdef IDLE_ADDR
			if ( pc == IDLE_ADDR + 1 )
				goto hit_idle_addr;
		#else
			if ( pc > 0x10000 )
			{
				pc = WORD( pc - 1 );
				s_time -= 11;
				goto loop;
			}
		#endif
	CASE7( C7, CF, D7, DF, E7, EF, F7 ):
		data = pc;
		pc = opcode & 0x38;
		#ifdef RST_BASE
			pc += RST_BASE;
		#endif
		goto push_data;

// PUSH/POP
	case 0xF5: // PUSH AF
		data = r.b.a * 0x100u + flags;
		goto push_data;
	
	case 0xC5: // PUSH BC
	case 0xD5: // PUSH DE
	case 0xE5: // PUSH HL
		data = R16( opcode, 4, 0xC5 );
	push_data:
		sp = WORD( sp - 2 );
		WRITE_WORD( sp, data );
		goto loop;
	
	case 0xF1: // POP AF
		flags = READ_MEM( sp );
		r.b.a = READ_MEM( (sp + 1) );
		sp = WORD( sp + 2 );
		goto loop;
	
	case 0xC1: // POP BC
	case 0xD1: // POP DE
	case 0xE1: // POP HL
		R16( opcode, 4, 0xC1 ) = READ_WORD( sp );
		sp = WORD( sp + 2 );
		goto loop;
	
// ADC/ADD/SBC/SUB
	case 0x96: // SUB (HL)
	case 0x86: // ADD (HL)
		flags &= ~C01;
	case 0x9E: // SBC (HL)
	case 0x8E: // ADC (HL)
		data = READ_MEM( r.w.hl );
		goto adc_data;
	
	case 0xD6: // SUB A,imm
	case 0xC6: // ADD imm
		flags &= ~C01;
	case 0xDE: // SBC A,imm
	case 0xCE: // ADC imm
		pc++;
		goto adc_data;
	
	CASE7( 90, 91, 92, 93, 94, 95, 97 ): // SUB r
	CASE7( 80, 81, 82, 83, 84, 85, 87 ): // ADD r
		flags &= ~C01;
	CASE7( 98, 99, 9A, 9B, 9C, 9D, 9F ): // SBC r
	CASE7( 88, 89, 8A, 8B, 8C, 8D, 8F ): // ADC r
		data = R8( opcode & 7, 0 );
	adc_data: {
		int result = data + (flags & C01);
		data ^= r.b.a;
		flags = opcode >> 3 & N02; // bit 4 is set in subtract opcodes
		if ( flags )
			result = -result;
		result += r.b.a;
		data ^= result;
		flags +=(data & H10) +
				((data + 0x80) >> 6 & V04) +
				SZ28C( result & 0x1FF );
		r.b.a = result;
		goto loop;
	}

// CP
	case 0xBE: // CP (HL)
		data = READ_MEM( r.w.hl );
		goto cp_data;
	
	case 0xFE: // CP imm
		pc++;
		goto cp_data;
	
	CASE7( B8, B9, BA, BB, BC, BD, BF ): // CP r
		data = R8( opcode, 0xB8 );
	cp_data: {
		int result = r.b.a - data;
		flags = N02 + (data & (F20 | F08)) + (result >> 8 & C01);
		data ^= r.b.a;
		flags +=(((result ^ r.b.a) & data) >> 5 & V04) +
				(((data & H10) ^ result) & (S80 | H10));
		if ( BYTE( result ) )
			goto loop;
		flags += Z40;
		goto loop;
	}
	
// ADD HL,r.w
	
	case 0x39: // ADD HL,SP
		data = sp;
		goto add_hl_data;
	
	case 0x09: // ADD HL,BC
	case 0x19: // ADD HL,DE
	case 0x29: // ADD HL,HL
		data = R16( opcode, 4, 0x09 );
	add_hl_data: {
		int sum = r.w.hl + data;
		data ^= r.w.hl;
		r.w.hl = sum;
		flags = (flags & (S80 | Z40 | V04)) +
				(sum >> 16) +
				(sum >> 8 & (F20 | F08)) +
				((data ^ sum) >> 8 & H10);
		goto loop;
	}
	
	case 0x27:{// DAA
		int a = r.b.a;
		if ( a > 0x99 )
			flags |= C01;
		
		int adjust = 0x60 * (flags & C01);
		
		if ( flags & H10 || (a & 0x0F) > 9 )
			adjust += 0x06;
		
		if ( flags & N02 )
			adjust = -adjust;
		a += adjust;
		
		flags = (flags & (C01 | N02)) +
				((r.b.a ^ a) & H10) +
				SZ28P( BYTE( a ) );
		r.b.a = a;
		goto loop;
	}
	
// INC/DEC
	case 0x34: // INC (HL)
		data = READ_MEM( r.w.hl ) + 1;
		WRITE_MEM( r.w.hl, data );
		goto inc_set_flags;
	
	CASE7( 04, 0C, 14, 1C, 24, 2C, 3C ): // INC r
		data = ++R8( opcode >> 3, 0 );
	inc_set_flags:
		flags = (flags & C01) +
				(((data & 0x0F) - 1) & H10) +
				SZ28( BYTE( data ) );
		if ( data != 0x80 )
			goto loop;
		flags += V04;
		goto loop;
	
	case 0x35: // DEC (HL)
		data = READ_MEM( r.w.hl ) - 1;
		WRITE_MEM( r.w.hl, data );
		goto dec_set_flags;
	
	CASE7( 05, 0D, 15, 1D, 25, 2D, 3D ): // DEC r
		data = --R8( opcode >> 3, 0 );
	dec_set_flags:
		flags = (flags & C01) + N02 +
				(((data & 0x0F) + 1) & H10) +
				SZ28( BYTE( data ) );
		if ( data != 0x7F )
			goto loop;
		flags += V04;
		goto loop;

	case 0x03: // INC BC
	case 0x13: // INC DE
	case 0x23: // INC HL
		R16( opcode, 4, 0x03 )++;
		goto loop;
	
	case 0x33: // INC SP
		sp = WORD( sp + 1 );
		goto loop;
	
	case 0x0B: // DEC BC
	case 0x1B: // DEC DE
	case 0x2B: // DEC HL
		R16( opcode, 4, 0x0B )--;
		goto loop;
	
	case 0x3B: // DEC SP
		sp = WORD( sp - 1 );
		goto loop;
	
// AND
	case 0xA6: // AND (HL)
		data = READ_MEM( r.w.hl );
		goto and_data;
	
	case 0xE6: // AND imm
		pc++;
		goto and_data;
	
	CASE7( A0, A1, A2, A3, A4, A5, A7 ): // AND r
		data = R8( opcode, 0xA0 );
	and_data:
		r.b.a &= data;
		flags = SZ28P( r.b.a ) + H10;
		goto loop;
	
// OR
	case 0xB6: // OR (HL)
		data = READ_MEM( r.w.hl );
		goto or_data;
	
	case 0xF6: // OR imm
		pc++;
		goto or_data;
	
	CASE7( B0, B1, B2, B3, B4, B5, B7 ): // OR r
		data = R8( opcode, 0xB0 );
	or_data:
		r.b.a |= data;
		flags = SZ28P( r.b.a );
		goto loop;

// XOR
	case 0xAE: // XOR (HL)
		data = READ_MEM( r.w.hl );
		goto xor_data;
	
	case 0xEE: // XOR imm
		pc++;
		goto xor_data;
	
	CASE7( A8, A9, AA, AB, AC, AD, AF ): // XOR r
		data = R8( opcode, 0xA8 );
	xor_data:
		r.b.a ^= data;
		flags = SZ28P( r.b.a );
		goto loop;

// LD
	CASE7( 70, 71, 72, 73, 74, 75, 77 ): // LD (HL),r
		WRITE_MEM( r.w.hl, R8( opcode, 0x70 ) );
		goto loop;
	
	CASE6( 41, 42, 43, 44, 45, 47 ): // LD B,r
	CASE6( 48, 4A, 4B, 4C, 4D, 4F ): // LD C,r
	CASE6( 50, 51, 53, 54, 55, 57 ): // LD D,r
	CASE6( 58, 59, 5A, 5C, 5D, 5F ): // LD E,r
	CASE6( 60, 61, 62, 63, 65, 67 ): // LD H,r
	CASE6( 68, 69, 6A, 6B, 6C, 6F ): // LD L,r
	CASE6( 78, 79, 7A, 7B, 7C, 7D ): // LD A,r
		R8( opcode >> 3 & 7, 0 ) = R8( opcode & 7, 0 );
		goto loop;
	
	CASE5( 06, 0E, 16, 1E, 26 ): // LD r,imm
		R8( opcode >> 3, 0 ) = data;
		pc++;
		goto loop;
	
	case 0x36: // LD (HL),imm
		pc++;
		WRITE_MEM( r.w.hl, data );
		goto loop;
	
	CASE7( 46, 4E, 56, 5E, 66, 6E, 7E ): // LD r,(HL)
		R8( opcode >> 3, 8 ) = READ_MEM( r.w.hl );
		goto loop;
	
	case 0x01: // LD r.w,imm
	case 0x11:
	case 0x21:
		R16( opcode, 4, 0x01 ) = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x31: // LD sp,imm
		sp = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x2A:{// LD HL,(addr)
		int addr = GET_ADDR();
		pc += 2;
		r.w.hl = READ_WORD( addr );
		goto loop;
	}
	
	case 0x32:{// LD (addr),A
		int addr = GET_ADDR();
		pc += 2;
		WRITE_MEM( addr, r.b.a );
		goto loop;
	}
	
	case 0x22:{// LD (addr),HL
		int addr = GET_ADDR();
		pc += 2;
		WRITE_WORD( addr, r.w.hl );
		goto loop;
	}
	
	case 0x02: // LD (BC),A
	case 0x12: // LD (DE),A
		WRITE_MEM( R16( opcode, 4, 0x02 ), r.b.a );
		goto loop;
	
	case 0x0A: // LD A,(BC)
	case 0x1A: // LD A,(DE)
		r.b.a = READ_MEM( R16( opcode, 4, 0x0A ) );
		goto loop;
	
	case 0xF9: // LD SP,HL
		sp = r.w.hl;
		goto loop;
	
// Rotate
	
	case 0x07:{// RLCA
		int temp = r.b.a;
		temp = (temp << 1) + (temp >> 7);
		flags = (flags & (S80 | Z40 | P04)) +
				(temp & (F20 | F08 | C01));
		r.b.a = temp;
		goto loop;
	}
	
	case 0x0F:{// RRCA
		int temp = r.b.a;
		flags = (flags & (S80 | Z40 | P04)) +
				(temp & C01);
		temp = (temp << 7) + (temp >> 1);
		flags += temp & (F20 | F08);
		r.b.a = temp;
		goto loop;
	}
	
	case 0x17:{// RLA
		int temp = (r.b.a << 1) + (flags & C01);
		flags = (flags & (S80 | Z40 | P04)) +
				(temp & (F20 | F08)) +
				(temp >> 8);
		r.b.a = temp;
		goto loop;
	}
	
	case 0x1F:{// RRA
		int temp = (flags << 7) + (r.b.a >> 1);
		flags = (flags & (S80 | Z40 | P04)) +
				(temp & (F20 | F08)) +
				(r.b.a & C01);
		r.b.a = temp;
		goto loop;
	}
	
// Misc
	case 0x2F:{// CPL
		int temp = ~r.b.a;
		flags = (flags & (S80 | Z40 | P04 | C01)) +
				(temp & (F20 | F08)) +
				(H10 | N02);
		r.b.a = temp;
		goto loop;
	}
	
	case 0x3F:{// CCF
		flags = ((flags & (S80 | Z40 | P04 | C01)) ^ C01) +
				(flags << 4 & H10) +
				(r.b.a & (F20 | F08));
		goto loop;
	}
	
	case 0x37: // SCF
		flags = (flags & (S80 | Z40 | P04)) | C01 +
				(r.b.a & (F20 | F08));
		goto loop;
	
	case 0xDB: // IN A,(imm)
		pc++;
		r.b.a = IN_PORT( (data + r.b.a * 0x100) );
		goto loop;

	case 0xE3:{// EX (SP),HL
		int temp = READ_WORD( sp );
		WRITE_WORD( sp, r.w.hl );
		r.w.hl = temp;
		goto loop;
	}
	
	case 0xEB: // EX DE,HL
		EX( r.w.hl, r.w.de );
		goto loop;
	
	case 0xD9: // EXX DE,HL
		EXX( w.bc );
		EXX( w.de );
		EXX( w.hl );
		goto loop;
	
	case 0xF3: // DI
		R.iff1 = 0;
		R.iff2 = 0;
		goto loop;
	
	case 0xFB: // EI
		R.iff1 = 1;
		R.iff2 = 1;
		// TODO: delayed effect
		goto loop;
	
	case 0x76: // HALT
		goto halt;
	
//////////////////////////////////////// CB prefix
	{
	case 0xCB:
		pc++;
		switch ( data )
		{
	
	// Rotate left
		
	#define RLC( read, write ) {\
		int result = read;\
		result = BYTE( result << 1 ) + (result >> 7);\
		flags = SZ28P( result ) + (result & C01);\
		write;\
		goto loop;\
	}
		
		case 0x06: // RLC (HL)
			s_time += 7;
			data = r.w.hl;
		rlc_data_addr:
			RLC( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 00, 01, 02, 03, 04, 05, 07 ):{// RLC r
			byte& reg = R8( data, 0 );
			RLC( reg, reg = result )
		}
		
	#define RL( read, write ) {\
		int result = (read << 1) + (flags & C01);\
		flags = SZ28PC( result );\
		write;\
		goto loop;\
	}
		
		case 0x16: // RL (HL)
			s_time += 7;
			data = r.w.hl;
		rl_data_addr:
			RL( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 10, 11, 12, 13, 14, 15, 17 ):{// RL r
			byte& reg = R8( data, 0x10 );
			RL( reg, reg = result )
		}
		
	#define SLA( read, low_bit, write ) {\
		int result = (read << 1) + low_bit;\
		flags = SZ28PC( result );\
		write;\
		goto loop;\
	}
		
		case 0x26: // SLA (HL)
			s_time += 7;
			data = r.w.hl;
		sla_data_addr:
			SLA( READ_MEM( data ), 0, WRITE_MEM( data, result ) )
		
		CASE7( 20, 21, 22, 23, 24, 25, 27 ):{// SLA r
			byte& reg = R8( data, 0x20 );
			SLA( reg, 0, reg = result )
		}
		
		case 0x36: // SLL (HL)
			s_time += 7;
			data = r.w.hl;
		sll_data_addr:
			SLA( READ_MEM( data ), 1, WRITE_MEM( data, result ) )
		
		CASE7( 30, 31, 32, 33, 34, 35, 37 ):{// SLL r
			byte& reg = R8( data, 0x30 );
			SLA( reg, 1, reg = result )
		}
		
	// Rotate right
		
	#define RRC( read, write ) {\
		int result = read;\
		flags = result & C01;\
		result = BYTE( result << 7 ) + (result >> 1);\
		flags += SZ28P( result );\
		write;\
		goto loop;\
	}
		
		case 0x0E: // RRC (HL)
			s_time += 7;
			data = r.w.hl;
		rrc_data_addr:
			RRC( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 08, 09, 0A, 0B, 0C, 0D, 0F ):{// RRC r
			byte& reg = R8( data, 0x08 );
			RRC( reg, reg = result )
		}
		
	#define RR( read, write ) {\
		int result = read;\
		int temp = result & C01;\
		result = BYTE( flags << 7 ) + (result >> 1);\
		flags = SZ28P( result ) + temp;\
		write;\
		goto loop;\
	}
		
		case 0x1E: // RR (HL)
			s_time += 7;
			data = r.w.hl;
		rr_data_addr:
			RR( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 18, 19, 1A, 1B, 1C, 1D, 1F ):{// RR r
			byte& reg = R8( data, 0x18 );
			RR( reg, reg = result )
		}
		
	#define SRA( read, write ) {\
		int result = read;\
		flags = result & C01;\
		result = (result & 0x80) + (result >> 1);\
		flags += SZ28P( result );\
		write;\
		goto loop;\
	}
		
		case 0x2E: // SRA (HL)
			data = r.w.hl;
			s_time += 7;
		sra_data_addr:
			SRA( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 28, 29, 2A, 2B, 2C, 2D, 2F ):{// SRA r
			byte& reg = R8( data, 0x28 );
			SRA( reg, reg = result )
		}
		
	#define SRL( read, write ) {\
		int result = read;\
		flags = result & C01;\
		result >>= 1;\
		flags += SZ28P( result );\
		write;\
		goto loop;\
	}
		
		case 0x3E: // SRL (HL)
			s_time += 7;
			data = r.w.hl;
		srl_data_addr:
			SRL( READ_MEM( data ), WRITE_MEM( data, result ) )
		
		CASE7( 38, 39, 3A, 3B, 3C, 3D, 3F ):{// SRL r
			byte& reg = R8( data, 0x38 );
			SRL( reg, reg = result )
		}
		
	// BIT
		{
			int temp;
		CASE8( 46, 4E, 56, 5E, 66, 6E, 76, 7E ): // BIT b,(HL)
			s_time += 4;
			temp = READ_MEM( r.w.hl );
			flags &= C01;
			goto bit_temp;
		CASE7( 40, 41, 42, 43, 44, 45, 47 ): // BIT 0,r
		CASE7( 48, 49, 4A, 4B, 4C, 4D, 4F ): // BIT 1,r
		CASE7( 50, 51, 52, 53, 54, 55, 57 ): // BIT 2,r
		CASE7( 58, 59, 5A, 5B, 5C, 5D, 5F ): // BIT 3,r
		CASE7( 60, 61, 62, 63, 64, 65, 67 ): // BIT 4,r
		CASE7( 68, 69, 6A, 6B, 6C, 6D, 6F ): // BIT 5,r
		CASE7( 70, 71, 72, 73, 74, 75, 77 ): // BIT 6,r
		CASE7( 78, 79, 7A, 7B, 7C, 7D, 7F ): // BIT 7,r
			temp = R8( data & 7, 0 );
			flags = (flags & C01) + (temp & (F20 | F08));
		bit_temp:
			temp = temp & (1 << (data >> 3 & 7));
			flags += (temp & S80) + H10;
			flags += (unsigned) --temp >> 8 & (Z40 | P04);
			goto loop;
		}
		
	// SET/RES
		CASE8( 86, 8E, 96, 9E, A6, AE, B6, BE ): // RES b,(HL)
		CASE8( C6, CE, D6, DE, E6, EE, F6, FE ):{// SET b,(HL)
			s_time += 7;
			int temp = READ_MEM( r.w.hl );
			int bit = 1 << (data >> 3 & 7);
			temp |= bit; // SET
			if ( !(data & 0x40) )
				temp ^= bit; // RES
			WRITE_MEM( r.w.hl, temp );
			goto loop;
		}
		
		CASE7( C0, C1, C2, C3, C4, C5, C7 ): // SET 0,r
		CASE7( C8, C9, CA, CB, CC, CD, CF ): // SET 1,r
		CASE7( D0, D1, D2, D3, D4, D5, D7 ): // SET 2,r
		CASE7( D8, D9, DA, DB, DC, DD, DF ): // SET 3,r
		CASE7( E0, E1, E2, E3, E4, E5, E7 ): // SET 4,r
		CASE7( E8, E9, EA, EB, EC, ED, EF ): // SET 5,r
		CASE7( F0, F1, F2, F3, F4, F5, F7 ): // SET 6,r
		CASE7( F8, F9, FA, FB, FC, FD, FF ): // SET 7,r
			R8( data & 7, 0 ) |= 1 << (data >> 3 & 7);
			goto loop;
		
		CASE7( 80, 81, 82, 83, 84, 85, 87 ): // RES 0,r
		CASE7( 88, 89, 8A, 8B, 8C, 8D, 8F ): // RES 1,r
		CASE7( 90, 91, 92, 93, 94, 95, 97 ): // RES 2,r
		CASE7( 98, 99, 9A, 9B, 9C, 9D, 9F ): // RES 3,r
		CASE7( A0, A1, A2, A3, A4, A5, A7 ): // RES 4,r
		CASE7( A8, A9, AA, AB, AC, AD, AF ): // RES 5,r
		CASE7( B0, B1, B2, B3, B4, B5, B7 ): // RES 6,r
		CASE7( B8, B9, BA, BB, BC, BD, BF ): // RES 7,r
			R8( data & 7, 0 ) &= ~(1 << (data >> 3 & 7));
			goto loop;
		}
		assert( false );
	}

#undef GET_ADDR
#define GET_ADDR()  GET_LE16( &INSTR( 1, pc ) )

//////////////////////////////////////// ED prefix
	{
	case 0xED:
		pc++;
		s_time += (clock_table + 256) [data] >> 4;
		switch ( data )
		{
		{
			int temp;
		case 0x72: // SBC HL,SP
		case 0x7A: // ADC HL,SP
			temp = sp;
			if ( 0 )
		case 0x42: // SBC HL,BC
		case 0x52: // SBC HL,DE
		case 0x62: // SBC HL,HL
		case 0x4A: // ADC HL,BC
		case 0x5A: // ADC HL,DE
		case 0x6A: // ADC HL,HL
				temp = R16( data >> 3 & 6, 1, 0 );
			int sum = temp + (flags & C01);
			flags = ~data >> 2 & N02;
			if ( flags )
				sum = -sum;
			sum += r.w.hl;
			temp ^= r.w.hl;
			temp ^= sum;
			flags +=(sum >> 16 & C01) +
					(temp >> 8 & H10) +
					(sum >> 8 & (S80 | F20 | F08)) +
					((temp + 0x8000) >> 14 & V04);
			r.w.hl = sum;
			if ( WORD( sum ) )
				goto loop;
			flags += Z40;
			goto loop;
		}
		
		CASE8( 40, 48, 50, 58, 60, 68, 70, 78 ):{// IN r,(C)
			int temp = IN_PORT( r.w.bc );
			R8( data >> 3, 8 ) = temp;
			flags = (flags & C01) + SZ28P( temp );
			goto loop;
		}
		
		case 0x71: // OUT (C),0
			r.b.flags = 0;
		CASE7( 41, 49, 51, 59, 61, 69, 79 ): // OUT (C),r
			OUT_PORT( r.w.bc, R8( data >> 3, 8 ) );
			goto loop;
		
		{
			int temp;
		case 0x73: // LD (ADDR),SP
			temp = sp;
			if ( 0 )
		case 0x43: // LD (ADDR),BC
		case 0x53: // LD (ADDR),DE
				temp = R16( data, 4, 0x43 );
			int addr = GET_ADDR();
			pc += 2;
			WRITE_WORD( addr, temp );
			goto loop;
		}
		
		case 0x4B: // LD BC,(ADDR)
		case 0x5B:{// LD DE,(ADDR)
			int addr = GET_ADDR();
			pc += 2;
			R16( data, 4, 0x4B ) = READ_WORD( addr );
			goto loop;
		}
		
		case 0x7B:{// LD SP,(ADDR)
			int addr = GET_ADDR();
			pc += 2;
			sp = READ_WORD( addr );
			goto loop;
		}
		
		case 0x67:{// RRD
			int temp = READ_MEM( r.w.hl );
			WRITE_MEM( r.w.hl, ((r.b.a << 4) + (temp >> 4)) );
			temp = (r.b.a & 0xF0) + (temp & 0x0F);
			flags = (flags & C01) + SZ28P( temp );
			r.b.a = temp;
			goto loop;
		}
		
		case 0x6F:{// RLD
			int temp = READ_MEM( r.w.hl );
			WRITE_MEM( r.w.hl, ((temp << 4) + (r.b.a & 0x0F)) );
			temp = (r.b.a & 0xF0) + (temp >> 4);
			flags = (flags & C01) + SZ28P( temp );
			r.b.a = temp;
			goto loop;
		}
		
		CASE8( 44, 4C, 54, 5C, 64, 6C, 74, 7C ): // NEG
			opcode = 0x10; // flag to do SBC instead of ADC
			flags &= ~C01;
			data = r.b.a;
			r.b.a = 0;
			goto adc_data;
		
		{
			int inc;
		case 0xA9: // CPD
		case 0xB9: // CPDR
			inc = -1;
			if ( 0 )
		case 0xA1: // CPI
		case 0xB1: // CPIR
				inc = +1;
			int addr = r.w.hl;
			r.w.hl = addr + inc;
			int temp = READ_MEM( addr );
			
			int result = r.b.a - temp;
			flags = (flags & C01) + N02 +
					((((temp ^ r.b.a) & H10) ^ result) & (S80 | H10));
			
			if ( !BYTE( result ) )
				flags += Z40;
			result -= (flags & H10) >> 4;
			flags += result & F08;
			flags += result << 4 & F20;
			if ( !--r.w.bc )
				goto loop;
			
			flags += V04;
			if ( flags & Z40 || data < 0xB0 )
				goto loop;
			
			pc -= 2;
			s_time += 5;
			goto loop;
		}
		
		{
			int inc;
		case 0xA8: // LDD
		case 0xB8: // LDDR
			inc = -1;
			if ( 0 )
		case 0xA0: // LDI
		case 0xB0: // LDIR
				inc = +1;
			int addr = r.w.hl;
			r.w.hl = addr + inc;
			int temp = READ_MEM( addr );
			
			addr = r.w.de;
			r.w.de = addr + inc;
			WRITE_MEM( addr, temp );
			
			temp += r.b.a;
			flags = (flags & (S80 | Z40 | C01)) +
					(temp & F08) + (temp << 4 & F20);
			if ( !--r.w.bc )
				goto loop;
			
			flags += V04;
			if ( data < 0xB0 )
				goto loop;
			
			pc -= 2;
			s_time += 5;
			goto loop;
		}
		
		{
			int inc;
		case 0xAB: // OUTD
		case 0xBB: // OTDR
			inc = -1;
			if ( 0 )
		case 0xA3: // OUTI
		case 0xB3: // OTIR
				inc = +1;
			int addr = r.w.hl;
			r.w.hl = addr + inc;
			int temp = READ_MEM( addr );
			
			int b = --r.b.b;
			flags = (temp >> 6 & N02) + SZ28( b );
			if ( b && data >= 0xB0 )
			{
				pc -= 2;
				s_time += 5;
			}
			
			OUT_PORT( r.w.bc, temp );
			goto loop;
		}
		
		{
			int inc;
		case 0xAA: // IND
		case 0xBA: // INDR
			inc = -1;
			if ( 0 )
		case 0xA2: // INI
		case 0xB2: // INIR
				inc = +1;
			
			int addr = r.w.hl;
			r.w.hl = addr + inc;
			
			int temp = IN_PORT( r.w.bc );
			
			int b = --r.b.b;
			flags = (temp >> 6 & N02) + SZ28( b );
			if ( b && data >= 0xB0 )
			{
				pc -= 2;
				s_time += 5;
			}
			
			WRITE_MEM( addr, temp );
			goto loop;
		}
		
		case 0x47: // LD I,A
			R.i = r.b.a;
			goto loop;
		
		case 0x4F: // LD R,A
			SET_R( r.b.a );
			dprintf( "LD R,A not supported\n" );
			warning = true;
			goto loop;
		
		case 0x57: // LD A,I
			r.b.a = R.i;
			goto ld_ai_common;
		
		case 0x5F: // LD A,R
			r.b.a = GET_R();
			dprintf( "LD A,R not supported\n" );
			warning = true;
		ld_ai_common:
			flags = (flags & C01) + SZ28( r.b.a ) + (R.iff2 << 2 & V04);
			goto loop;
		
		CASE8( 45, 4D, 55, 5D, 65, 6D, 75, 7D ): // RETI/RETN
			R.iff1 = R.iff2;
			goto ret_taken;
		
		case 0x46: case 0x4E: case 0x66: case 0x6E: // IM 0
			R.im = 0;
			goto loop;
		
		case 0x56: case 0x76: // IM 1
			R.im = 1;
			goto loop;
		
		case 0x5E: case 0x7E: // IM 2
			R.im = 2;
			goto loop;
		
		default:
			dprintf( "Opcode $ED $%02X not supported\n", data );
			warning = true;
			goto loop;
		}
		assert( false );
	}

//////////////////////////////////////// DD/FD prefix
	{
	int ixy;
	case 0xDD:
		ixy = ix;
		goto ix_prefix;
	case 0xFD:
		ixy = iy;
	ix_prefix:
		pc++;
		int data2 = READ_CODE( pc );
		s_time += (clock_table + 256) [data] & 0x0F;
		switch ( data )
		{
	// TODO: more efficient way of avoid negative address
	// TODO: avoid using this as argument to READ_MEM() since it is evaluated twice
	#define IXY_DISP( ixy, disp )   WORD( (ixy ) + (disp))
	
	#define SET_IXY( in ) if ( opcode == 0xDD ) ix = in; else iy = in;
	
	// ADD/ADC/SUB/SBC
	
		case 0x96: // SUB (IXY+disp)
		case 0x86: // ADD (IXY+disp)
			flags &= ~C01;
		case 0x9E: // SBC (IXY+disp)
		case 0x8E: // ADC (IXY+disp)
			pc++;
			opcode = data;
			data = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) ) );
			goto adc_data;
		
		case 0x94: // SUB HXY
		case 0x84: // ADD HXY
			flags &= ~C01;
		case 0x9C: // SBC HXY
		case 0x8C: // ADC HXY
			opcode = data;
			data = ixy >> 8;
			goto adc_data;
		
		case 0x95: // SUB LXY
		case 0x85: // ADD LXY
			flags &= ~C01;
		case 0x9D: // SBC LXY
		case 0x8D: // ADC LXY
			opcode = data;
			data = BYTE( ixy );
			goto adc_data;
		
		{
			int temp;
		case 0x39: // ADD IXY,SP
			temp = sp;
			goto add_ixy_data;
		
		case 0x29: // ADD IXY,HL
			temp = ixy;
			goto add_ixy_data;
		
		case 0x09: // ADD IXY,BC
		case 0x19: // ADD IXY,DE
			temp = R16( data, 4, 0x09 );
		add_ixy_data: {
			int sum = ixy + temp;
			temp ^= ixy;
			ixy = WORD( sum );
			flags = (flags & (S80 | Z40 | V04)) +
					(sum >> 16) +
					(sum >> 8 & (F20 | F08)) +
					((temp ^ sum) >> 8 & H10);
			goto set_ixy;
		}
		}
	
	// AND
		case 0xA6: // AND (IXY+disp)
			pc++;
			data = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) ) );
			goto and_data;
		
		case 0xA4: // AND HXY
			data = ixy >> 8;
			goto and_data;
		
		case 0xA5: // AND LXY
			data = BYTE( ixy );
			goto and_data;
	
	// OR
		case 0xB6: // OR (IXY+disp)
			pc++;
			data = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) ) );
			goto or_data;
		
		case 0xB4: // OR HXY
			data = ixy >> 8;
			goto or_data;
		
		case 0xB5: // OR LXY
			data = BYTE( ixy );
			goto or_data;
	
	// XOR
		case 0xAE: // XOR (IXY+disp)
			pc++;
			data = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) ) );
			goto xor_data;
		
		case 0xAC: // XOR HXY
			data = ixy >> 8;
			goto xor_data;
		
		case 0xAD: // XOR LXY
			data = BYTE( ixy );
			goto xor_data;
	
	// CP
		case 0xBE: // CP (IXY+disp)
			pc++;
			data = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) )  );
			goto cp_data;
		
		case 0xBC: // CP HXY
			data = ixy >> 8;
			goto cp_data;
		
		case 0xBD: // CP LXY
			data = BYTE( ixy );
			goto cp_data;
		
	// LD
		CASE7( 70, 71, 72, 73, 74, 75, 77 ): // LD (IXY+disp),r
			data = R8( data, 0x70 );
			if ( 0 )
		case 0x36: // LD (IXY+disp),imm
				pc++, data = READ_CODE( pc );
			pc++;
			WRITE_MEM( IXY_DISP( ixy, SBYTE( data2 ) ), data );
			goto loop;

		CASE5( 44, 4C, 54, 5C, 7C ): // LD r,HXY
			R8( data >> 3, 8 ) = ixy >> 8;
			goto loop;
		
		case 0x64: // LD HXY,HXY
		case 0x6D: // LD LXY,LXY
			goto loop;
		
		CASE5( 45, 4D, 55, 5D, 7D ): // LD r,LXY
			R8( data >> 3, 8 ) = ixy;
			goto loop;
		
		CASE7( 46, 4E, 56, 5E, 66, 6E, 7E ): // LD r,(IXY+disp)
			pc++;
			R8( data >> 3, 8 ) = READ_MEM( IXY_DISP( ixy, SBYTE( data2 ) ) );
			goto loop;
		
		case 0x26: // LD HXY,imm
			pc++;
			goto ld_hxy_data;
			
		case 0x65: // LD HXY,LXY
			data2 = BYTE( ixy );
			goto ld_hxy_data;
		
		CASE5( 60, 61, 62, 63, 67 ): // LD HXY,r
			data2 = R8( data, 0x60 );
		ld_hxy_data:
			ixy = BYTE( ixy ) + (data2 << 8);
			goto set_ixy;
		
		case 0x2E: // LD LXY,imm
			pc++;
			goto ld_lxy_data;
			
		case 0x6C: // LD LXY,HXY
			data2 = ixy >> 8;
			goto ld_lxy_data;
		
		CASE5( 68, 69, 6A, 6B, 6F ): // LD LXY,r
			data2 = R8( data, 0x68 );
		ld_lxy_data:
			ixy = (ixy & 0xFF00) + data2;
		set_ixy:
			if ( opcode == 0xDD )
			{
				ix = ixy;
				goto loop;
			}
			iy = ixy;
			goto loop;

		case 0xF9: // LD SP,IXY
			sp = ixy;
			goto loop;
	
		case 0x22:{// LD (ADDR),IXY
			int addr = GET_ADDR();
			pc += 2;
			WRITE_WORD( addr, ixy );
			goto loop;
		}
		
		case 0x21: // LD IXY,imm
			ixy = GET_ADDR();
			pc += 2;
			goto set_ixy;
		
		case 0x2A:{// LD IXY,(addr)
			int addr = GET_ADDR();
			ixy = READ_WORD( addr );
			pc += 2;
			goto set_ixy;
		}
		
	// DD/FD CB prefix
		case 0xCB: {
			data = IXY_DISP( ixy, SBYTE( data2 ) );
			pc++;
			data2 = READ_CODE( pc );
			pc++;
			switch ( data2 )
			{
			case 0x06: goto rlc_data_addr; // RLC (IXY)
			case 0x16: goto rl_data_addr;  // RL (IXY)
			case 0x26: goto sla_data_addr; // SLA (IXY)
			case 0x36: goto sll_data_addr; // SLL (IXY)
			case 0x0E: goto rrc_data_addr; // RRC (IXY)
			case 0x1E: goto rr_data_addr;  // RR (IXY)
			case 0x2E: goto sra_data_addr; // SRA (IXY)
			case 0x3E: goto srl_data_addr; // SRL (IXY)
			
			CASE8( 46, 4E, 56, 5E, 66, 6E, 76, 7E ):{// BIT b,(IXY+disp)
				int temp = READ_MEM( data );
				temp = temp & (1 << (data2 >> 3 & 7));
				flags = (flags & C01) + H10 + (temp & S80);
				flags += (unsigned) --temp >> 8 & (Z40 | P04);
				goto loop;
			}
			
			CASE8( 86, 8E, 96, 9E, A6, AE, B6, BE ): // RES b,(IXY+disp)
			CASE8( C6, CE, D6, DE, E6, EE, F6, FE ):{// SET b,(IXY+disp)
				int temp = READ_MEM( data );
				int bit = 1 << (data2 >> 3 & 7);
				temp |= bit; // SET
				if ( !(data2 & 0x40) )
					temp ^= bit; // RES
				WRITE_MEM( data, temp );
				goto loop;
			}
			
			default:
				dprintf( "Opcode $%02X $CB $%02X not supported\n", opcode, data2 );
				warning = true;
				goto loop;
			}
			assert( false );
		}
		
	// INC/DEC
		case 0x23: // INC IXY
			ixy = WORD( ixy + 1 );
			goto set_ixy;
		
		case 0x2B: // DEC IXY
			ixy = WORD( ixy - 1 );
			goto set_ixy;
		
		case 0x34: // INC (IXY+disp)
			ixy = IXY_DISP( ixy, SBYTE( data2 ) );
			pc++;
			data = READ_MEM( ixy ) + 1;
			WRITE_MEM( ixy, data );
			goto inc_set_flags;
		
		case 0x35: // DEC (IXY+disp)
			ixy = IXY_DISP( ixy, SBYTE( data2 ) );
			pc++;
			data = READ_MEM( ixy ) - 1;
			WRITE_MEM( ixy, data );
			goto dec_set_flags;
		
		case 0x24: // INC HXY
			ixy = WORD( ixy + 0x100 );
			data = ixy >> 8;
			goto inc_xy_common;
		
		case 0x2C: // INC LXY
			data = BYTE( ixy + 1 );
			ixy = (ixy & 0xFF00) + data;
		inc_xy_common:
			if ( opcode == 0xDD )
			{
				ix = ixy;
				goto inc_set_flags;
			}
			iy = ixy;
			goto inc_set_flags;
		
		case 0x25: // DEC HXY
			ixy = WORD( ixy - 0x100 );
			data = ixy >> 8;
			goto dec_xy_common;
		
		case 0x2D: // DEC LXY
			data = BYTE( ixy - 1 );
			ixy = (ixy & 0xFF00) + data;
		dec_xy_common:
			if ( opcode == 0xDD )
			{
				ix = ixy;
				goto dec_set_flags;
			}
			iy = ixy;
			goto dec_set_flags;
		
	// PUSH/POP
		case 0xE5: // PUSH IXY
			data = ixy;
			goto push_data;
		
		case 0xE1:{// POP IXY
			ixy = READ_WORD( sp );
			sp = WORD( sp + 2 );
			goto set_ixy;
		}
	
	// Misc
		
		case 0xE9: // JP (IXY)
			pc = ixy;
			goto loop;
		
		case 0xE3:{// EX (SP),IXY
			int temp = READ_WORD( sp );
			WRITE_WORD( sp, ixy );
			ixy = temp;
			goto set_ixy;
		}
		
		default:
			dprintf( "Unnecessary DD/FD prefix encountered\n" );
			warning = true;
			pc--;
			goto loop;
		}
		assert( false );
	}
	
	}
	dprintf( "Unhandled main opcode: $%02X\n", opcode );
	assert( false );
	
#ifdef IDLE_ADDR
hit_idle_addr:
	s_time -= 11;
	goto out_of_time;
#endif
halt:
	s_time &= 3; // increment by multiple of 4
out_of_time:
	pc--;
	
	r.b.flags = flags;
	R.ix     = ix;
	R.iy     = iy;
	R.sp     = sp;
	R.pc     = pc;
	R.b      = r.b;
	
	CPU.cpu_state_.base = s.base;
	CPU.cpu_state_.time = s_time;
	CPU.cpu_state = &CPU.cpu_state_;
}
