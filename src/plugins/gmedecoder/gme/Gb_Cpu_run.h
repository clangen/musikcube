// Game_Music_Emu $vers. http://www.slack.net/~ant/

#if 0
/* Define these macros in the source file before #including this file.
- Parameters might be expressions, so they are best evaluated only once,
though they NEVER have side-effects, so multiple evaluation is OK.
- Output parameters might be a multiple-assignment expression like "a=x",
so they must NOT be parenthesized.
- Macros "returning" void may use a {} statement block. */

	// 0 <= addr <= 0xFFFF + page_size
	// time functions can be used
	int  READ_MEM(  addr_t );
	void WRITE_MEM( addr_t, int data );
	
	// Access of 0xFF00 + offset
	// 0 <= offset <= 0xFF
	int  READ_IO(  int offset );
	void WRITE_IO( int offset, int data );

	// Often-used instructions use this instead of READ_MEM
	void READ_FAST( addr_t, int& out );

// The following can be used within macros:
	
	// Current time
	cpu_time_t TIME();
#endif

/* Copyright (C) 2003-2009 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

// Common instructions:
//
// 365880   FA      LD   A,(nn)
// 355863   20      JR   NZ
// 313655   21      LD   HL,nn
// 274580   28      JR   Z
// 252878   FE      CP   n
// 230541   7E      LD   A,(HL)
// 226209   2A      LD   A,(HL+)
// 217467   CD      CALL
// 212034   C9      RET
// 208376   CB      CB prefix
//
//  27486   CB 7E   BIT  7,(HL)
//  15925   CB 76   BIT  6,(HL)
//  13035   CB 19   RR   C
//  11557   CB 7F   BIT  7,A
//  10898   CB 37   SWAP A
//  10208   CB 66   BIT  4,(HL)

// Allows MWCW debugger to step through code properly
#ifdef CPU_BEGIN
	CPU_BEGIN
#endif

#define TIME()  s.time

#define CODE_PAGE( addr )   s.code_map [GB_CPU_PAGE( addr )]
#define READ_CODE( addr )   (CODE_PAGE( addr ) [GB_CPU_OFFSET( addr )])

// Flags with hex value for clarity when used as mask.
// Stored in indicated variable during emulation.
int const z80 = 0x80; // cz
int const n40 = 0x40; // ph
int const h20 = 0x20; // ph
int const c10 = 0x10; // cz

#define SET_FLAGS( in )\
{\
	cz = ((in) << 4 & 0x100) + (~(in) >> 7 & 1);\
	ph = (~(in) << 2 & 0x100) + ((in) >> 1 & 0x10);\
}

// random bits in cz to catch misuse of them
#define SET_FLAGS_DEBUG( in )\
{\
	cz = ((in) << 4 & 0x100) | (rand() & ~0x1FF) | ((in) & 0x80 ? 0 : (rand() & 0xFF) | 1);\
	ph = (~(in) << 2 & 0x100) | (((in) >> 1 & 0x10) ^ BYTE( cz ));\
}

#define GET_FLAGS( out )\
{\
	out = (cz >> 4 & c10);\
	out += ~ph >> 2 & n40;\
	out += (ph ^ cz) << 1 & h20;\
	if ( !BYTE( cz ) )\
		out += z80;\
}

#define CC_NZ() ( BYTE( cz ))
#define CC_Z()  (!BYTE( cz ))
#define CC_NC() (!(cz & 0x100))
#define CC_C()  (  cz & 0x100 )

// Truncation
#define BYTE(  n ) ((BOOST::uint8_t ) (n)) /* (unsigned) n & 0xFF */
#define SBYTE( n ) ((BOOST::int8_t  ) (n)) /* (BYTE( n ) ^ 0x80) - 0x80 */
#define WORD(  n ) ((BOOST::uint16_t) (n)) /* (unsigned) n & 0xFFFF */

{
	Gb_Cpu::cpu_state_t s;
	CPU.cpu_state = &s;
	memcpy( &s, &CPU.cpu_state_, sizeof s );
	
	union {
		struct {
		#if BLARGG_BIG_ENDIAN
			byte b, c, d, e, h, l, flags, a;
		#else
			byte c, b, e, d, l, h, a, flags;
		#endif
		} rg; // individual registers
		Gb_Cpu::core_regs_t rp; // pairs
		
		byte r8_ [8]; // indexed registers (use R8 macro due to endian dependence)
		BOOST::uint16_t r16 [4]; // indexed pairs
	};
	BLARGG_STATIC_ASSERT( sizeof rg == 8 && sizeof rp == 8 );

	#if BLARGG_BIG_ENDIAN
		#define R8( n ) (r8_ [n]) 
	#elif BLARGG_LITTLE_ENDIAN
		#define R8( n ) (r8_ [(n) ^ 1]) 
	#else
		// Be sure "blargg_endian.h" has been #included in the file that #includes this
		#error "Byte order of CPU must be known"
	#endif
	
	rp = CPU.r;
	int pc = CPU.r.pc;
	int sp = CPU.r.sp;
	int ph;
	int cz;
	SET_FLAGS( rg.flags );
	
	int time = s.time;
	
loop:
	
	check( (unsigned) pc < 0x10000 + 1 ); // +1 so emulator can catch wrap-around
	check( (unsigned) sp < 0x10000 );
	
	byte const* instr = CODE_PAGE( pc );
	int op;
	
	if ( GB_CPU_OFFSET(~0) == ~0 )
	{
		op = instr [pc];
		pc++;
		instr += pc;
	}
	else
	{
		instr += GB_CPU_OFFSET( pc );
		op = *instr++;
		pc++;
	}
	
#define GET_ADDR()  GET_LE16( instr )
	
	static byte const instr_times [256*2] = {
	//   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		 4,12, 8, 8, 4, 4, 8, 4,20, 8, 8, 8, 4, 4, 8, 4,// 0
		 4,12, 8, 8, 4, 4, 8, 4,12, 8, 8, 8, 4, 4, 8, 4,// 1
		 8,12, 8, 8, 4, 4, 8, 4, 8, 8, 8, 8, 4, 4, 8, 4,// 2
		 8,12, 8, 8,12,12,12, 4, 8, 8, 8, 8, 4, 4, 8, 4,// 3
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// 4
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// 5
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// 6
		 8, 8, 8, 8, 8, 8, 0, 8, 4, 4, 4, 4, 4, 4, 8, 4,// 7
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// 8
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// 9
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// A
		 4, 4, 4, 4, 4, 4, 8, 4, 4, 4, 4, 4, 4, 4, 8, 4,// B
		 8,12,16,16,12,16, 8,16, 8,16,16, 0,12,24, 8,16,// C
		 8,12,16, 0,12,16, 8,16, 8,16,16, 0,12, 0, 8,16,// D
		12,12, 8, 0, 0,16, 8,16,16, 4,16, 0, 0, 0, 8,16,// E
		12,12, 8, 4, 0,16, 8,16,12, 8,16, 4, 0, 0, 8,16,// F
		
	// CB prefixed
	//   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 0
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 1
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 2
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 3
		 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,// 4
		 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,// 5
		 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,// 6
		 8, 8, 8, 8, 8, 8,12, 8, 8, 8, 8, 8, 8, 8,12, 8,// 7
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 8
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// 9
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// A
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// B
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// C
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// D
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// E
		 8, 8, 8, 8, 8, 8,16, 8, 8, 8, 8, 8, 8, 8,16, 8,// F
	};
	
	if ( time >= 0 )
		goto stop;
	
	time += instr_times [op];
	
	int data;
	data = *instr;
	s.time = time;
	
	#ifdef CPU_INSTR_HOOK
	{ CPU_INSTR_HOOK( (pc-1), (instr-1), rg.a, rp.bc, rp.de, rp.hl, sp ); }
	#endif
	
	switch ( op )
	{

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH_( cond, clocks )\
{\
	pc++;\
	if ( !(cond) )\
		goto loop;\
	pc = WORD( pc + SBYTE( data ) );\
	time += clocks;\
	goto loop;\
}

#define BRANCH( cond ) BRANCH_( cond, 4 )

// Most Common

	case 0x20: // JR NZ
		BRANCH( CC_NZ() )
	
	case 0x21: // LD HL,IMM (common)
		rp.hl = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x28: // JR Z
		BRANCH( CC_Z() )
	
	case 0xF2: // LD A,(0xFF00+C)
		READ_IO( rg.c, rg.a );
		goto loop;
	
	case 0xF0: // LD A,(0xFF00+imm)
		pc++;
		READ_IO( data, rg.a );
		goto loop;
	
	{
		int temp;
	case 0x0A: // LD A,(BC)
		temp = rp.bc;
		goto ld_a_ind_comm;
	
	case 0x3A: // LD A,(HL-)
		temp = rp.hl;
		rp.hl = temp - 1;
		goto ld_a_ind_comm;
	
	case 0x1A: // LD A,(DE)
		temp = rp.de;
		goto ld_a_ind_comm;
	
	case 0x2A: // LD A,(HL+) (common)
		temp = rp.hl;
		rp.hl = temp + 1;
		goto ld_a_ind_comm;
		
	case 0xFA: // LD A,IND16 (common)
		temp = GET_ADDR();
		pc += 2;
	ld_a_ind_comm:
		READ_FAST( temp, rg.a );
		goto loop;
	}
	
	{
		int temp;
	case 0xBE: // CP (HL)
		temp = READ_MEM( rp.hl );
		goto cmp_comm;
	
	case 0xB8: // CP B
	case 0xB9: // CP C
	case 0xBA: // CP D
	case 0xBB: // CP E
	case 0xBC: // CP H
	case 0xBD: // CP L
	case 0xBF: // CP A
		temp = R8( op & 7 );
	cmp_comm:
		ph = rg.a ^ temp; // N=1 H=*
		cz = rg.a - temp; // C=* Z=*
		goto loop;
	}
	
	case 0xFE: // CP IMM
		pc++;
		ph = rg.a ^ data; // N=1 H=*
		cz = rg.a - data; // C=* Z=*
		goto loop;
	
	case 0x46: // LD B,(HL)
	case 0x4E: // LD C,(HL)
	case 0x56: // LD D,(HL)
	case 0x5E: // LD E,(HL)
	case 0x66: // LD H,(HL)
	case 0x6E: // LD L,(HL)
	case 0x7E:{// LD A,(HL)
		int addr = rp.hl;
		READ_FAST( addr, R8( op >> 3 & 7 ) );
		goto loop;
	}
	
	case 0xC4: // CNZ (next-most-common)
		pc += 2;
		if ( CC_Z() )
			goto loop;
	call:
		time += 12;
		pc -= 2;
	case 0xCD: // CALL (most-common)
		data = pc + 2;
		pc = GET_ADDR();
	push: {
		int addr = WORD( sp - 1 );
		WRITE_MEM( addr, (data >> 8) );
		sp = WORD( sp - 2 );
		WRITE_MEM( sp, data );
		goto loop;
	}
	
	case 0xC8: // RET Z (next-most-common)
		if ( CC_NZ() )
			goto loop;
	ret:
		time += 12;
	case 0xD9: // RETI
	case 0xC9:{// RET (most common)
		pc = READ_MEM( sp );
		int addr = sp + 1;
		sp = WORD( sp + 2 );
		pc += 0x100 * READ_MEM( addr );
		goto loop;
	}
	
	case 0x00: // NOP
	case 0x40: // LD B,B
	case 0x49: // LD C,C
	case 0x52: // LD D,D
	case 0x5B: // LD E,E
	case 0x64: // LD H,H
	case 0x6D: // LD L,L
	case 0x7F: // LD A,A
		goto loop;
	
// CB Instructions

	case 0xCB:
		time += (instr_times + 256) [data];
		pc++;
		// now data is the opcode
		switch ( data ) {
			
		case 0x46: // BIT b,(HL)
		case 0x4E:
		case 0x56:
		case 0x5E:
		case 0x66:
		case 0x6E:
		case 0x76:
		case 0x7E: {
			int addr = rp.hl;
			READ_FAST( addr, op );
			goto bit_comm;
		}
		
		case 0x40: case 0x41: case 0x42: case 0x43: // BIT b,r
		case 0x44: case 0x45: case 0x47: case 0x48:
		case 0x49: case 0x4A: case 0x4B: case 0x4C:
		case 0x4D: case 0x4F: case 0x50: case 0x51:
		case 0x52: case 0x53: case 0x54: case 0x55:
		case 0x57: case 0x58: case 0x59: case 0x5A:
		case 0x5B: case 0x5C: case 0x5D: case 0x5F:
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x67: case 0x68:
		case 0x69: case 0x6A: case 0x6B: case 0x6C:
		case 0x6D: case 0x6F: case 0x70: case 0x71:
		case 0x72: case 0x73: case 0x74: case 0x75:
		case 0x77: case 0x78: case 0x79: case 0x7A:
		case 0x7B: case 0x7C: case 0x7D: case 0x7F:
			op = R8( data & 7 );
		bit_comm:
			ph = op >> (data >> 3 & 7) & 1;
			cz = (cz & 0x100) + ph;
			ph ^= 0x110; // N=0 H=1
			goto loop;
		
		case 0x86: // RES b,(HL)
		case 0x8E:
		case 0x96:
		case 0x9E:
		case 0xA6:
		case 0xAE:
		case 0xB6:
		case 0xBE: {
			int temp = READ_MEM( rp.hl );
			temp &= ~(1 << (data >> 3 & 7));
			WRITE_MEM( rp.hl, temp );
			goto loop;
		}
		
		case 0xC6: // SET b,(HL)
		case 0xCE:
		case 0xD6:
		case 0xDE:
		case 0xE6:
		case 0xEE:
		case 0xF6:
		case 0xFE: {
			int temp = READ_MEM( rp.hl );
			temp |= 1 << (data >> 3 & 7);
			WRITE_MEM( rp.hl, temp );
			goto loop;
		}
		
		case 0xC0: case 0xC1: case 0xC2: case 0xC3: // SET b,r
		case 0xC4: case 0xC5: case 0xC7: case 0xC8:
		case 0xC9: case 0xCA: case 0xCB: case 0xCC:
		case 0xCD: case 0xCF: case 0xD0: case 0xD1:
		case 0xD2: case 0xD3: case 0xD4: case 0xD5:
		case 0xD7: case 0xD8: case 0xD9: case 0xDA:
		case 0xDB: case 0xDC: case 0xDD: case 0xDF:
		case 0xE0: case 0xE1: case 0xE2: case 0xE3:
		case 0xE4: case 0xE5: case 0xE7: case 0xE8:
		case 0xE9: case 0xEA: case 0xEB: case 0xEC:
		case 0xED: case 0xEF: case 0xF0: case 0xF1:
		case 0xF2: case 0xF3: case 0xF4: case 0xF5:
		case 0xF7: case 0xF8: case 0xF9: case 0xFA:
		case 0xFB: case 0xFC: case 0xFD: case 0xFF:
			R8( data & 7 ) |= 1 << (data >> 3 & 7);
			goto loop;

		case 0x80: case 0x81: case 0x82: case 0x83: // RES b,r
		case 0x84: case 0x85: case 0x87: case 0x88:
		case 0x89: case 0x8A: case 0x8B: case 0x8C:
		case 0x8D: case 0x8F: case 0x90: case 0x91:
		case 0x92: case 0x93: case 0x94: case 0x95:
		case 0x97: case 0x98: case 0x99: case 0x9A:
		case 0x9B: case 0x9C: case 0x9D: case 0x9F:
		case 0xA0: case 0xA1: case 0xA2: case 0xA3:
		case 0xA4: case 0xA5: case 0xA7: case 0xA8:
		case 0xA9: case 0xAA: case 0xAB: case 0xAC:
		case 0xAD: case 0xAF: case 0xB0: case 0xB1:
		case 0xB2: case 0xB3: case 0xB4: case 0xB5:
		case 0xB7: case 0xB8: case 0xB9: case 0xBA:
		case 0xBB: case 0xBC: case 0xBD: case 0xBF:
			R8( data & 7 ) &= ~(1 << (data >> 3 & 7));
			goto loop;
		
		case 0x36: // SWAP (HL)
			op = READ_MEM( rp.hl );
			goto swap_comm;
		
		case 0x30: // SWAP B
		case 0x31: // SWAP C
		case 0x32: // SWAP D
		case 0x33: // SWAP E
		case 0x34: // SWAP H
		case 0x35: // SWAP L
		case 0x37: // SWAP A
			op = R8( data & 7 );
		swap_comm:
			op = (op >> 4) + (op << 4);
			cz = BYTE( op );
			ph = cz + 0x100;
			if ( data == 0x36 )
				goto write_hl_op_ff;
			R8( data & 7 ) = op;
			goto loop;
		
// Shift/Rotate

		case 0x26: // SLA (HL)
			cz = 0;
		case 0x16: // RL (HL)
			cz = (cz >> 8 & 1) + (READ_MEM( rp.hl ) << 1);
			goto rl_hl_common;
		
		case 0x06: // RLC (HL)
			cz = READ_MEM( rp.hl );
			cz = (cz << 1) + (cz >> 7 & 1);
		rl_hl_common:
			// Z=* C=*
			ph = cz | 0x100; // N=0 H=0
			WRITE_MEM( rp.hl, cz );
			goto loop;
		
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x27: // SLA r
			cz = 0;
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x17: // RL r
			cz = (cz >> 8 & 1) + (R8( data & 7 ) << 1);
			goto rl_common;
		
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x07: // RLC r
			cz = R8( data & 7 );
			cz = (cz << 1) + (cz >> 7 & 1);
		rl_common:
			// Z=* C=*
			ph = cz | 0x100; // N=0 H=0
			R8( data & 7 ) = cz;
			goto loop;
		
		case 0x0E: // RRC (HL)
			cz = READ_MEM( rp.hl );
			cz += cz << 8 & 0x100;
			goto rr_hl_common;
		
		case 0x2E: // SRA (HL)
			cz = READ_MEM( rp.hl );
			cz += cz << 1 & 0x100;
			goto rr_hl_common;
		
		case 0x3E: // SRL (HL)
			cz = 0;
		case 0x1E: // RR (HL)
			cz = (cz & 0x100) + READ_MEM( rp.hl );
		rr_hl_common:
			cz = (cz << 8) + (cz >> 1); // Z=* C=*
			ph = cz | 0x100; // N=0 H=0
			WRITE_MEM( rp.hl, cz );
			goto loop;
		
		case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0F: // RRC r
			cz = R8( data & 7 );
			cz += cz << 8 & 0x100;
			goto rr_common;
		
		case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2F: // SRA r
			cz = R8( data & 7 );
			cz += cz << 1 & 0x100;
			goto rr_common;
		
		case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3F: // SRL r
			cz = 0;
		case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1F: // RR r
			cz = (cz & 0x100) + R8( data & 7 );
		rr_common:
			cz = (cz << 8) + (cz >> 1); // Z=* C=*
			ph = cz | 0x100; // N=0 H=0
			R8( data & 7 ) = cz;
			goto loop;
		
	} // CB op
	assert( false ); // unhandled CB op
	
	case 0x07: // RLCA
		cz = rg.a >> 7;
		goto rlc_common;
	case 0x17: // RLA
		cz = cz >> 8 & 1;
	rlc_common:
		cz  += rg.a << 1;
		ph   = cz | 0x100;
		rg.a = BYTE( cz );
		cz  |= 1;
		goto loop;
	
	case 0x0F: // RRCA
		ph = rg.a << 8;
		goto rrc_common;
	case 0x1F: // RRA
		ph = cz;
	rrc_common:
		cz = (rg.a << 8) + 1; // Z=0 C=*
		rg.a = ((ph & 0x100) + rg.a) >> 1;
		ph = 0x100; // N=0 H=0
		goto loop;

// Load

	case 0x70: // LD (HL),B
	case 0x71: // LD (HL),C
	case 0x72: // LD (HL),D
	case 0x73: // LD (HL),E
	case 0x74: // LD (HL),H
	case 0x75: // LD (HL),L
	case 0x77: // LD (HL),A
		op = R8( op & 7 );
	write_hl_op_ff:
		WRITE_MEM( rp.hl, op );
		goto loop;

	case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // LD r,r
	case 0x48: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4F:
	case 0x50: case 0x51: case 0x53: case 0x54: case 0x55: case 0x57:
	case 0x58: case 0x59: case 0x5A: case 0x5C: case 0x5D: case 0x5F:
	case 0x60: case 0x61: case 0x62: case 0x63: case 0x65: case 0x67:
	case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6F:
	case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D:
		R8( op >> 3 & 7 ) = R8( op & 7 );
		goto loop;

	case 0x08: // LD IND16,SP
		data = GET_ADDR();
		pc += 2;
		WRITE_MEM( data, sp );
		data++;
		WRITE_MEM( data, (sp >> 8) );
		goto loop;
	
	case 0xF9: // LD SP,HL
		sp = rp.hl;
		goto loop;

	case 0x31: // LD SP,IMM
		sp = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x01: // LD BC,IMM
	case 0x11: // LD DE,IMM
		r16 [(unsigned) op >> 4] = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0xE2: // LD (0xFF00+C),A
		WRITE_IO( rg.c, rg.a );
		goto loop;
	
	case 0xE0: // LD (0xFF00+imm),A
		pc++;
		WRITE_IO( data, rg.a );
		goto loop;
	
	{
		int temp;
	case 0x32: // LD (HL-),A
		temp = rp.hl;
		rp.hl = temp - 1;
		goto write_data_rg_a;
	
	case 0x02: // LD (BC),A
		temp = rp.bc;
		goto write_data_rg_a;
	
	case 0x12: // LD (DE),A
		temp = rp.de;
		goto write_data_rg_a;
	
	case 0x22: // LD (HL+),A
		temp = rp.hl;
		rp.hl = temp + 1;
		goto write_data_rg_a;
		
	case 0xEA: // LD IND16,A (common)
		temp = GET_ADDR();
		pc += 2;
	write_data_rg_a:
		WRITE_MEM( temp, rg.a );
		goto loop;
	}
	
	case 0x06: // LD B,IMM
		rg.b = data;
		pc++;
		goto loop;
	
	case 0x0E: // LD C,IMM
		rg.c = data;
		pc++;
		goto loop;
	
	case 0x16: // LD D,IMM
		rg.d = data;
		pc++;
		goto loop;
	
	case 0x1E: // LD E,IMM
		rg.e = data;
		pc++;
		goto loop;
	
	case 0x26: // LD H,IMM
		rg.h = data;
		pc++;
		goto loop;
	
	case 0x2E: // LD L,IMM
		rg.l = data;
		pc++;
		goto loop;
	
	case 0x36: // LD (HL),IMM
		WRITE_MEM( rp.hl, data );
		pc++;
		goto loop;
	
	case 0x3E: // LD A,IMM
		rg.a = data;
		pc++;
		goto loop;

// Increment/decrement

	case 0x03: // INC BC
	case 0x13: // INC DE
	case 0x23: // INC HL
		r16 [(unsigned) op >> 4]++;
		goto loop;
	
	case 0x33: // INC SP
		sp = WORD( sp + 1 );
		goto loop;

	case 0x0B: // DEC BC
	case 0x1B: // DEC DE
	case 0x2B: // DEC HL
		r16 [(unsigned) op >> 4]--;
		goto loop;
	
	case 0x3B: // DEC SP
		sp = WORD( sp - 1 );
		goto loop;
	
	case 0x34: // INC (HL)
		op = rp.hl;
		data = READ_MEM( op );
		data++;
		WRITE_MEM( op, data );
		goto inc_comm;
	
	case 0x04: // INC B
	case 0x0C: // INC C (common)
	case 0x14: // INC D
	case 0x1C: // INC E
	case 0x24: // INC H
	case 0x2C: // INC L
	case 0x3C: // INC A
		op = op >> 3 & 7;
		data = R8( op ) + 1;
		R8( op ) = data;
	inc_comm:
		ph = data - 0x101; // N=0 H=*
		cz = (cz & 0x100) + BYTE( data ); // C=- Z=*
		goto loop;
	
	case 0x35: // DEC (HL)
		op = rp.hl;
		data = READ_MEM( op );
		data--;
		WRITE_MEM( op, data );
		goto dec_comm;
	
	case 0x05: // DEC B
	case 0x0D: // DEC C
	case 0x15: // DEC D
	case 0x1D: // DEC E
	case 0x25: // DEC H
	case 0x2D: // DEC L
	case 0x3D: // DEC A
		op = op >> 3 & 7;
		data = R8( op ) - 1;
		R8( op ) = data;
	dec_comm:
		ph = data + 1; // N=1 H=*
		cz = (cz & 0x100) + BYTE( data ); // C=- Z=*
		goto loop;

// Add 16-bit

	case 0xF8: // LD  HL,SP+n
	case 0xE8:{// ADD SP,n
		pc++;
		int t = WORD( sp + SBYTE( data ) );
		cz = ((BYTE( sp ) + data) & 0x100) + 1; // Z=0 C=*
		ph = (sp ^ data ^ t) | 0x100; // N=0 H=*
		if ( op == 0xF8 )
		{
			rp.hl = t;
			goto loop;
		}
		sp = t;
		goto loop;
	}

	case 0x39: // ADD HL,SP
		data = sp;
		goto add_hl_comm;
	
	case 0x09: // ADD HL,BC
	case 0x19: // ADD HL,DE
	case 0x29: // ADD HL,HL
		data = r16 [(unsigned) op >> 4];
	add_hl_comm:
		ph = rp.hl ^ data;
		data += rp.hl;
		rp.hl = WORD( data );
		ph ^= data;
		cz = BYTE( cz ) + (data >> 8 & 0x100); // C=* Z=-
		ph = ((ph >> 8) ^ cz) | 0x100; // N=0 H=*
		goto loop;
	
	case 0x86: // ADD (HL)
		data = READ_MEM( rp.hl );
		goto add_comm;
	
	case 0x80: // ADD B
	case 0x81: // ADD C
	case 0x82: // ADD D
	case 0x83: // ADD E
	case 0x84: // ADD H
	case 0x85: // ADD L
	case 0x87: // ADD A
		data = R8( op & 7 );
		goto add_comm;
	
	case 0xC6: // ADD IMM
		pc++;
	add_comm:
		ph   = (rg.a ^ data) | 0x100; // N=1 H=*
		cz   = rg.a + data; // C=* Z=*
		rg.a = cz;
		goto loop;
	
// Add/Subtract

	case 0x8E: // ADC (HL)
		data = READ_MEM( rp.hl );
		goto adc_comm;
	
	case 0x88: // ADC B
	case 0x89: // ADC C
	case 0x8A: // ADC D
	case 0x8B: // ADC E
	case 0x8C: // ADC H
	case 0x8D: // ADC L
	case 0x8F: // ADC A
		data = R8( op & 7 );
		goto adc_comm;
	
	case 0xCE: // ADC IMM
		pc++;
	adc_comm:
		ph   = (rg.a ^ data) | 0x100; // N=1 H=*
		cz   = rg.a + data + (cz >> 8 & 1); // C=* Z=*
		rg.a = cz;
		goto loop;
	
	case 0x96: // SUB (HL)
		data = READ_MEM( rp.hl );
		goto sub_comm;
	
	case 0x90: // SUB B
	case 0x91: // SUB C
	case 0x92: // SUB D
	case 0x93: // SUB E
	case 0x94: // SUB H
	case 0x95: // SUB L
	case 0x97: // SUB A
		data = R8( op & 7 );
		goto sub_comm;
	
	case 0xD6: // SUB IMM
		pc++;
	sub_comm:
		ph   = rg.a ^ data; // N=1 H=*
		cz   = rg.a - data; // C=* Z=*
		rg.a = cz;
		goto loop;
	
	case 0x9E: // SBC (HL)
		data = READ_MEM( rp.hl );
		goto sbc_comm;
	
	case 0x98: // SBC B
	case 0x99: // SBC C
	case 0x9A: // SBC D
	case 0x9B: // SBC E
	case 0x9C: // SBC H
	case 0x9D: // SBC L
	case 0x9F: // SBC A
		data = R8( op & 7 );
		goto sbc_comm;
	
	case 0xDE: // SBC IMM
		pc++;
	sbc_comm:
		ph   = rg.a ^ data; // N=1 H=*
		cz   = rg.a - data - (cz >> 8 & 1); // C=* Z=*
		rg.a = cz;
		goto loop;

// Logical

	case 0xA0: // AND B
	case 0xA1: // AND C
	case 0xA2: // AND D
	case 0xA3: // AND E
	case 0xA4: // AND H
	case 0xA5: // AND L
		data = R8( op & 7 );
		goto and_comm;
	
	case 0xA6: // AND (HL)
		data = READ_MEM( rp.hl );
		goto and_comm;
	case 0xE6: // AND IMM
		pc++;
	and_comm:
		cz = rg.a & data; // C=0 Z=*
		ph = ~cz; // N=0 H=1
		rg.a = cz;
		goto loop;
	
	case 0xA7: // AND A
		cz = rg.a; // C=0 Z=*
		ph = ~rg.a; // N=0 H=1
		goto loop;

	case 0xB0: // OR B
	case 0xB1: // OR C
	case 0xB2: // OR D
	case 0xB3: // OR E
	case 0xB4: // OR H
	case 0xB5: // OR L
		data = R8( op & 7 );
		goto or_comm;
	
	case 0xB6: // OR (HL)
		data = READ_MEM( rp.hl );
		goto or_comm;
	case 0xF6: // OR IMM
		pc++;
	or_comm:
		cz = rg.a | data; // C=0 Z=*
		ph = cz | 0x100; // N=0 H=0
		rg.a = cz;
		goto loop;
	
	case 0xB7: // OR A
		cz = rg.a; // C=0 Z=*
		ph = rg.a + 0x100; // N=0 H=0
		goto loop;

	case 0xA8: // XOR B
	case 0xA9: // XOR C
	case 0xAA: // XOR D
	case 0xAB: // XOR E
	case 0xAC: // XOR H
	case 0xAD: // XOR L
		data = R8( op & 7 );
		goto xor_comm;
	
	case 0xAE: // XOR (HL)
		data = READ_MEM( rp.hl );
		pc--;
	case 0xEE: // XOR IMM
		pc++;
	xor_comm:
		cz = rg.a ^ data; // C=0 Z=*
		ph = cz + 0x100; // N=0 H=0
		rg.a = cz;
		goto loop;
	
	case 0xAF: // XOR A
		rg.a = 0;
		cz   = 0; // C=0 Z=*
		ph   = 0x100; // N=0 H=0
		goto loop;

// Stack

	case 0xF1: // POP AF
	case 0xC1: // POP BC
	case 0xD1: // POP DE
	case 0xE1: // POP HL (common)
		data = READ_MEM( sp );
		r16 [op >> 4 & 3] = data + 0x100 * READ_MEM( (sp + 1) );
		sp = WORD( sp + 2 );
		if ( op != 0xF1 )
			goto loop;
		
		SET_FLAGS( rg.a );
		rg.a = rg.flags;
		goto loop;
	
	case 0xC5: // PUSH BC
		data = rp.bc;
		goto push;
	
	case 0xD5: // PUSH DE
		data = rp.de;
		goto push;
	
	case 0xE5: // PUSH HL
		data = rp.hl;
		goto push;
	
	case 0xF5: // PUSH AF
		GET_FLAGS( data );
		data += rg.a << 8;
		goto push;

// Flow control
	
	case 0xFF: case 0xC7: case 0xCF: case 0xD7: // RST
	case 0xDF: case 0xE7: case 0xEF: case 0xF7:
		data = pc;
		pc = (op & 0x38) + CPU.rst_base;
		goto push;
	
	case 0xCC: // CALL Z
		pc += 2;
		if ( CC_Z() )
			goto call;
		goto loop;
	
	case 0xD4: // CALL NC
		pc += 2;
		if ( CC_NC() )
			goto call;
		goto loop;
	
	case 0xDC: // CALL C
		pc += 2;
		if ( CC_C() )
			goto call;
		goto loop;

	case 0xC0: // RET NZ
		if ( CC_NZ() )
			goto ret;
		goto loop;
	
	case 0xD0: // RET NC
		if ( CC_NC() )
			goto ret;
		goto loop;
	
	case 0xD8: // RET C
		if ( CC_C() )
			goto ret;
		goto loop;

	case 0x18: // JR
		BRANCH_( true, 0 )
	
	case 0x30: // JR NC
		BRANCH( CC_NC() )
	
	case 0x38: // JR C
		BRANCH( CC_C() )
	
	case 0xE9: // LD PC,HL
		pc = rp.hl;
		goto loop;

	case 0xC3: // JP (next-most-common)
		pc = GET_ADDR();
		goto loop;
	
	case 0xC2: // JP NZ
		pc += 2;
		if ( CC_NZ() )
			goto jp_taken;
		time -= 4;
		goto loop;
	
	case 0xCA: // JP Z (most common)
		pc += 2;
		if ( CC_Z() )
			goto jp_taken;
		time -= 4;
		goto loop;
	
	jp_taken:
		pc -= 2;
		pc = GET_ADDR();
		goto loop;
	
	case 0xD2: // JP NC
		pc += 2;
		if ( CC_NC() )
			goto jp_taken;
		time -= 4;
		goto loop;
	
	case 0xDA: // JP C
		pc += 2;
		if ( CC_C() )
			goto jp_taken;
		time -= 4;
		goto loop;

// Flags

	case 0x2F: // CPL
		rg.a = ~rg.a;
		ph = BYTE( ~cz ); // N=1 H=1
		goto loop;

	case 0x3F: // CCF
		ph = cz | 0x100; // N=0 H=0
		cz ^= 0x100; // C=* Z=-
		goto loop;

	case 0x37: // SCF
		ph = cz | 0x100; // N=0 H=0
		cz |= 0x100; // C=1 Z=-
		goto loop;

	case 0xF3: // DI
		goto loop;

	case 0xFB: // EI
		goto loop;

	case 0x27:{// DAA
		unsigned a = rg.a;
		int h = ph ^ cz;
		if ( ph & 0x100 )
		{
			if ( (h & 0x10) || (a & 0x0F) > 9 )
				a += 6;
			
			if ( (cz & 0x100) || a > 0x9F )
				a += 0x60;
		}
		else
		{
			if ( h & 0x10 )
				a = (a - 6) & 0xFF;
			
			if ( cz & 0x100 )
				a -= 0x60;
		}
		cz = (cz & 0x100) | a; // C=- Z=*
		rg.a = a;
		ph = (ph & 0x100) + BYTE( a ); // N=- H=0
		goto loop;
	}
	
// Special

	case 0x76: // HALT
	case 0x10: // STOP
	case 0xD3:            case 0xDB:            case 0xDD: // Illegal
	case 0xE3: case 0xE4: case 0xEB: case 0xEC: case 0xED: // (all freeze CPU)
	           case 0xF4:            case 0xFC: case 0xFD:
		goto stop;
	}
	
	// If this fails then an opcode isn't handled above
	assert( false );
	
stop:
	pc--;
	
	// copy state back
	CPU.cpu_state_.time = time;
	CPU.r.pc = pc;
	CPU.r.sp = sp;
	{
		int t;
		GET_FLAGS( t );
		rg.flags = t;
	}
	CPU.cpu_state = &CPU.cpu_state_;
	STATIC_CAST(Gb_Cpu::core_regs_t&,CPU.r) = rp;
}
