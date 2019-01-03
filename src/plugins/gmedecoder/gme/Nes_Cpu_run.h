// NES 6502 CPU emulator run function

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
	// 0 <= READ_MEM() <= 0xFF
	
	// 0 <= addr <= 0x1FF
	int  READ_LOW(  addr_t );
	void WRITE_LOW( addr_t, int data );
	// 0 <= READ_LOW() <= 0xFF

	// Often-used instructions attempt these before using a normal memory access.
	// Optional; defaults to READ_MEM() and WRITE_MEM()
	bool CAN_READ_FAST( addr_t ); // if true, uses result of READ_FAST
	void READ_FAST( addr_t, int& out ); // ALWAYS called BEFORE CAN_READ_FAST
	bool CAN_WRITE_FAST( addr_t ); // if true, uses WRITE_FAST instead of WRITE_MEM
	void WRITE_FAST( addr_t, int data );

	// Used by instructions most often used to access the NES PPU (LDA abs and BIT abs).
	// Optional; defaults to READ_MEM.
	void READ_PPU(  addr_t, int& out );
	// 0 <= out <= 0xFF

// The following can be used within macros:
	
	// Current time
	time_t TIME();
	
	// Allows use of time functions
	void FLUSH_TIME();
	
	// Must be used before end of macro if FLUSH_TIME() was used earlier
	void CACHE_TIME();

// Configuration (optional; commented behavior if defined)
	
	// Emulates dummy reads for indexed instructions
	#define NES_CPU_DUMMY_READS 1
	
	// Optimizes as if map_code( 0, 0x10000 + cpu_padding, FLAT_MEM ) is always in effect
	#define FLAT_MEM my_mem_array
	
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

// Allows MWCW debugger to step through code properly
#ifdef CPU_BEGIN
	CPU_BEGIN
#endif

// Time
#define TIME()          (s_time + s.base)
#define FLUSH_TIME()    {s.time = s_time - time_offset;}
#define CACHE_TIME()    {s_time = s.time + time_offset;}

// Defaults
#ifndef CAN_WRITE_FAST
	#define CAN_WRITE_FAST( addr )      0
	#define WRITE_FAST( addr, data )
#endif

#ifndef CAN_READ_FAST
	#define CAN_READ_FAST( addr )       0
	#define READ_FAST( addr, out )
#endif

#ifndef READ_PPU
	#define READ_PPU( addr, out )\
	{\
		FLUSH_TIME();\
		out = READ_MEM( addr );\
		CACHE_TIME();\
	}
#endif

#define READ_STACK  READ_LOW
#define WRITE_STACK WRITE_LOW

// Dummy reads
#if NES_CPU_DUMMY_READS
	// TODO: optimize time handling
	#define DUMMY_READ( addr, idx ) \
		if ( (addr & 0xFF) < idx )\
		{\
			int const time_offset = 1;\
			FLUSH_TIME();\
			READ_MEM( (addr - 0x100) );\
			CACHE_TIME();\
		}
#else
	#define DUMMY_READ( addr, idx )
#endif

// Code
#ifdef FLAT_MEM
	#define CODE_PAGE(   addr ) (FLAT_MEM)
	#define CODE_OFFSET( addr ) (addr)
#else
	#define CODE_PAGE( addr )   (s.code_map [NES_CPU_PAGE( addr )])
	#define CODE_OFFSET( addr ) NES_CPU_OFFSET( addr )
#endif
#define READ_CODE( addr )   (CODE_PAGE( addr ) [CODE_OFFSET( addr )])

// Stack
#define SET_SP( v ) (sp = ((v) + 1) | 0x100)
#define GET_SP()    ((sp - 1) & 0xFF)
#define SP( o )     ((sp + (o - (o>0)*0x100)) | 0x100)

// Truncation
#define BYTE(  n ) ((BOOST::uint8_t ) (n)) /* (unsigned) n & 0xFF */
#define SBYTE( n ) ((BOOST::int8_t  ) (n)) /* (BYTE( n ) ^ 0x80) - 0x80 */
#define WORD(  n ) ((BOOST::uint16_t) (n)) /* (unsigned) n & 0xFFFF */

// Flags with hex value for clarity when used as mask.
// Stored in indicated variable during emulation.
int const n80 = 0x80; // nz
int const v40 = 0x40; // flags
int const r20 = 0x20;
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

{
	int const time_offset = 0;
	
	// Local state
	Nes_Cpu::cpu_state_t s;
	#ifdef FLAT_MEM
		s.base = CPU.cpu_state_.base;
	#else
		s = CPU.cpu_state_;
	#endif
	CPU.cpu_state = &s;
	int s_time = CPU.cpu_state_.time; // helps even on x86
	
	// Registers
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
	
	// Check all values
	check( (unsigned) sp - 0x100 < 0x100 );
	check( (unsigned) pc < 0x10000 );
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
	
	// local to function in case it helps optimizer
	static byte const clock_table [256] =
	{// 0 1 2 3 4 5 6 7 8 9 A B C D E F
		0,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,// 0
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 1
		6,6,0,8,3,3,5,5,4,2,2,2,4,4,6,6,// 2
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 3
		6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,// 4
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 5
		6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,// 6
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 7
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// 8
		2,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,// 9
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// A
		2,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,// B
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// C
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// D
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// E
		2,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7 // F
	}; // 0x00 was 7 and 0x22 was 2
	
	// Update time
	if ( s_time >= 0 )
		goto out_of_time;
	
	#ifdef CPU_INSTR_HOOK
	{ CPU_INSTR_HOOK( (pc-1), (&instr [-1]), a, x, y, GET_SP(), TIME() ); }
	#endif
	
	s_time += clock_table [opcode];
	
	int data;
	data = *instr;
	
	switch ( opcode )
	{

// Macros

#define GET_MSB()       (instr [1])
#define ADD_PAGE( out ) (pc++, out = data + 0x100 * GET_MSB())
#define GET_ADDR()      GET_LE16( instr )

#define PAGE_PENALTY( lsb ) s_time += (lsb) >> 8;

#define INC_DEC( reg, n ) reg = BYTE( nz = reg + n ); goto loop;

#define IND_Y( cross, out ) {\
		int temp = READ_LOW( data ) + y;\
		out = temp + 0x100 * READ_LOW( BYTE( data + 1 ) );\
		cross( temp );\
	}
	
#define IND_X( out ) {\
		int temp = data + x;\
		out = 0x100 * READ_LOW( BYTE( temp + 1 ) ) + READ_LOW( BYTE( temp ) );\
	}
	
#define ARITH_ADDR_MODES( op )\
case op - 0x04: /* (ind,x) */\
	IND_X( data )\
	goto ptr##op;\
case op + 0x0C: /* (ind),y */\
	IND_Y( PAGE_PENALTY, data )\
	goto ptr##op;\
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

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH( cond )\
{\
	++pc;\
	if ( !(cond) ) goto loop;\
	s_time++;\
	int offset = SBYTE( data );\
	s_time += (BYTE(pc) + offset) >> 8 & 1;\
	pc = WORD( pc + offset );\
	goto loop;\
}

// Often-Used

	case 0xB5: // LDA zp,x
		a = nz = READ_LOW( BYTE( data + x ) );
		pc++;
		goto loop;
	
	case 0xA5: // LDA zp
		a = nz = READ_LOW( data );
		pc++;
		goto loop;
	
	case 0xD0: // BNE
		BRANCH( BYTE( nz ) );
	
	case 0x20: { // JSR
		int temp = pc + 1;
		pc = GET_ADDR();
		WRITE_STACK( SP( -1 ), temp >> 8 );
		sp = SP( -2 );
		WRITE_STACK( sp, temp );
		goto loop;
	}
	
	case 0x4C: // JMP abs
		pc = GET_ADDR();
		goto loop;
	
	case 0xE8: // INX
		INC_DEC( x, 1 )
	
	case 0x10: // BPL
		BRANCH( !IS_NEG )
	
	ARITH_ADDR_MODES( 0xC5 ) // CMP
		nz = a - data;
		pc++;
		c = ~nz;
		nz &= 0xFF;
		goto loop;
	
	case 0x30: // BMI
		BRANCH( IS_NEG )
	
	case 0xF0: // BEQ
		BRANCH( !BYTE( nz ) );
	
	case 0x95: // STA zp,x
		data = BYTE( data + x );
	case 0x85: // STA zp
		pc++;
		WRITE_LOW( data, a );
		goto loop;
	
	case 0xC8: // INY
		INC_DEC( y, 1 )

	case 0xA8: // TAY
		y  = a;
		nz = a;
		goto loop;
	
	case 0x98: // TYA
		a  = y;
		nz = y;
		goto loop;
	
	case 0xAD:{// LDA abs
		int addr = GET_ADDR();
		pc += 2;
		READ_PPU( addr, a = nz );
		goto loop;
	}
	
	case 0x60: // RTS
		pc = 1 + READ_STACK( sp );
		pc += 0x100 * READ_STACK( SP( 1 ) );
		sp = SP( 2 );
		goto loop;
	
	{
		int addr;
		
	case 0x8D: // STA abs
		addr = GET_ADDR();
		pc += 2;
		if ( CAN_WRITE_FAST( addr ) )
		{
			WRITE_FAST( addr, a );
			goto loop;
		}
	sta_ptr:
		FLUSH_TIME();
		WRITE_MEM( addr, a );
		CACHE_TIME();
		goto loop;
	
	case 0x99: // STA abs,Y
		addr = y + GET_ADDR();
		pc += 2;
		if ( CAN_WRITE_FAST( addr ) )
		{
			WRITE_FAST( addr, a );
			goto loop;
		}
		goto sta_abs_x;
	
	case 0x9D: // STA abs,X (slightly more common than STA abs)
		addr = x + GET_ADDR();
		pc += 2;
		if ( CAN_WRITE_FAST( addr ) )
		{
			WRITE_FAST( addr, a );
			goto loop;
		}
		DUMMY_READ( addr, x );
	sta_abs_x:
		FLUSH_TIME();
		WRITE_MEM( addr, a );
		CACHE_TIME();
		goto loop;
	
	case 0x91: // STA (ind),Y
		#define NO_PAGE_PENALTY( lsb )
		IND_Y( NO_PAGE_PENALTY, addr )
		pc++;
		DUMMY_READ( addr, y );
		goto sta_ptr;
	
	case 0x81: // STA (ind,X)
		IND_X( addr )
		pc++;
		goto sta_ptr;
	
	}
	
	case 0xA9: // LDA #imm
		pc++;
		a  = data;
		nz = data;
		goto loop;

	// common read instructions
	{
		int addr;
		
	case 0xA1: // LDA (ind,X)
		IND_X( addr )
		pc++;
		goto a_nz_read_addr;
	
	case 0xB1:// LDA (ind),Y
		addr = READ_LOW( data ) + y;
		PAGE_PENALTY( addr );
		addr += 0x100 * READ_LOW( BYTE( data + 1 ) );
		pc++;
		READ_FAST( addr, a = nz );
		if ( CAN_READ_FAST( addr ) )
			goto loop;
		DUMMY_READ( addr, y );
		goto a_nz_read_addr;
	
	case 0xB9: // LDA abs,Y
		PAGE_PENALTY( data + y );
		addr = GET_ADDR() + y;
		pc += 2;
		READ_FAST( addr, a = nz );
		if ( CAN_READ_FAST( addr ) )
			goto loop;
		goto a_nz_read_addr;
	
	case 0xBD: // LDA abs,X
		PAGE_PENALTY( data + x );
		addr = GET_ADDR() + x;
		pc += 2;
		READ_FAST( addr, a = nz );
		if ( CAN_READ_FAST( addr ) )
			goto loop;
		DUMMY_READ( addr, x );
	a_nz_read_addr:
		FLUSH_TIME();
		a = nz = READ_MEM( addr );
		CACHE_TIME();
		goto loop;
	
	}

// Branch

	case 0x50: // BVC
		BRANCH( !(flags & v40) )
	
	case 0x70: // BVS
		BRANCH( flags & v40 )
	
	case 0xB0: // BCS
		BRANCH( c & 0x100 )
	
	case 0x90: // BCC
		BRANCH( !(c & 0x100) )
	
// Load/store
	
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
	
	case 0xBE: // LDX abs,y
		data += y;
		PAGE_PENALTY( data );
	case 0xAE:{// LDX abs
		int addr = data + 0x100 * GET_MSB();
		pc += 2;
		FLUSH_TIME();
		x = nz = READ_MEM( addr );
		CACHE_TIME();
		goto loop;
	}
	
	{
		int temp;
	case 0x8C: // STY abs
		temp = y;
		goto store_abs;
	
	case 0x8E: // STX abs
		temp = x;
	store_abs:
		int addr = GET_ADDR();
		pc += 2;
		if ( CAN_WRITE_FAST( addr ) )
		{
			WRITE_FAST( addr, temp );
			goto loop;
		}
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
		nz &= 0xFF;
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
		nz &= 0xFF;
		goto loop;
	
// Logical

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
	
	case 0x2C:{// BIT abs
		int addr = GET_ADDR();
		pc += 2;
		READ_PPU( addr, nz );
		flags = (flags & ~v40) + (nz & v40);
		if ( a & nz )
			goto loop;
		nz <<= 8; // result must be zero, even if N bit is set
		goto loop;
	}
	
	case 0x24: // BIT zp
		nz = READ_LOW( data );
		pc++;
		flags = (flags & ~v40) + (nz & v40);
		if ( a & nz )
			goto loop; // Z should be clear, and nz must be non-zero if nz & a is
		nz <<= 8; // set Z flag without affecting N flag
		goto loop;
		
// Add/subtract

	ARITH_ADDR_MODES( 0xE5 ) // SBC
	case 0xEB: // unofficial equivalent
		data ^= 0xFF;
		goto adc_imm;
	
	ARITH_ADDR_MODES( 0x65 ) // ADC
	adc_imm: {
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
	
// Stack
	
	case 0x48: // PHA
		sp = SP( -1 );
		WRITE_STACK( sp, a );
		goto loop;
		
	case 0x68: // PLA
		a = nz = READ_STACK( sp );
		sp = SP( 1 );
		goto loop;
		
	case 0x40:{// RTI
		pc  = READ_STACK( SP( 1 ) );
		pc += READ_STACK( SP( 2 ) ) * 0x100;
		int temp = READ_STACK( sp );
		sp = SP( 3 );
		data = flags;
		SET_FLAGS( temp );
		CPU.r.flags = flags; // update externally-visible I flag
		int delta = s.base - CPU.irq_time_;
		if ( delta <= 0 ) goto loop; // end_time < irq_time
		if ( flags & i04 ) goto loop;
		s_time += delta;
		s.base = CPU.irq_time_;
		goto loop;
	}
	
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
		WRITE_STACK( sp, temp | (b10 | r20) );
		goto loop;
	}
	
	case 0x6C:{// JMP (ind)
		data = GET_ADDR();
		byte const* page = CODE_PAGE( data );
		pc = page [CODE_OFFSET( data )];
		data = (data & 0xFF00) + ((data + 1) & 0xFF);
		pc += page [CODE_OFFSET( data )] * 0x100;
		goto loop;
	}
	
	case 0x00: // BRK
		goto handle_brk;
	
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
			goto loop;
		}
		
		// TODO: implement
	delayed_cli:
		dprintf( "Delayed CLI not emulated\n" );
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
		
		dprintf( "Delayed SEI not emulated\n" );
		goto loop;
	}
	
// Unofficial
	
	// SKW - skip word
	case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
		PAGE_PENALTY( data + x );
	case 0x0C:
		pc++;
	// SKB - skip byte
	case 0x74: case 0x04: case 0x14: case 0x34: case 0x44: case 0x54: case 0x64:
	case 0x80: case 0x82: case 0x89: case 0xC2: case 0xD4: case 0xE2: case 0xF4:
		pc++;
		goto loop;
	
	// NOP
	case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
		goto loop;
	
	case Nes_Cpu::halt_opcode: // HLT - halt processor
		if ( pc-- > 0x10000 )
		{
			// handle wrap-around (assumes caller has put page of HLT at 0x10000)
			pc = WORD( pc );
			goto loop;
		}
	case 0x02: case 0x12:            case 0x32: case 0x42: case 0x52:
	case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2:
		goto stop;
	
// Unimplemented
	
	case 0xFF:  // force 256-entry jump table for optimization purposes
		c |= 1; // compiler doesn't know that this won't affect anything
	default:
		check( (unsigned) opcode < 0x100 );
		
		#ifdef UNIMPL_INSTR
			UNIMPL_INSTR();
		#endif
		
		// At least skip over proper number of bytes instruction uses
		static unsigned char const illop_lens [8] = {
			0x40, 0x40, 0x40, 0x80, 0x40, 0x40, 0x80, 0xA0
		};
		int opcode = instr [-1];
		int len = illop_lens [opcode >> 2 & 7] >> (opcode << 1 & 6) & 3;
		if ( opcode == 0x9C )
			len = 2;
		pc += len;
		CPU.error_count_++;
		
		// Account for extra clock
		if ( (opcode >> 4) == 0x0B )
		{
			if ( opcode == 0xB3 )
				data = READ_LOW( data );
			if ( opcode != 0xB7 )
				PAGE_PENALTY( data + y );
		}
		goto loop;
	}
	assert( false ); // catch missing 'goto loop' or accidental 'break'
	
	int result_;
handle_brk:
	pc++;
	result_ = b10 | 4;
	
#ifdef CPU_DONE
interrupt:
#endif
	{
		s_time += 7;
		
		// Save PC and read vector
		WRITE_STACK( SP( -1 ), pc >> 8 );
		WRITE_STACK( SP( -2 ), pc );
		pc = GET_LE16( &READ_CODE( 0xFFFA ) + (result_ & 4) );
		
		// Save flags
		int temp;
		GET_FLAGS( temp );
		temp |= r20 + (result_ & b10); // B flag set for BRK
		sp = SP( -3 );
		WRITE_STACK( sp, temp );
		
		// Update I flag in externally-visible flags
		CPU.r.flags = (flags |= i04);
		
		// Update time
		int delta = s.base - CPU.end_time_;
		if ( delta >= 0 )
			goto loop;
		s_time += delta;
		s.base = CPU.end_time_;
		goto loop;
	}
	
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
stop:
	
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
