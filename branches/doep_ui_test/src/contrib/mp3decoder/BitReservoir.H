/*
Copyright (c) 2002 Tony Million

This software is provided 'as-is', without any express or 
implied warranty. In no event will the authors be held liable 
for any damages arising from the use of this software.

Permission is granted to anyone to use this software for any 
purpose, including commercial applications, and to alter it 
and redistribute it freely, subject to the following 
restrictions:

1. The origin of this software must not be 
misrepresented; you must not claim that you wrote 
the original software. If you use this software in a 
product, an acknowledgment in the product 
documentation is required.

2. Altered source versions must be plainly marked as 
such, and must not be misrepresented as being the 
original software.

3. This notice may not be removed or altered from any 
source distribution.
*/

#pragma once

#include "BitStream.h"

#define BITSTREAM_BUFSIZE	4096
#define BITSTREAM_BUFSIZE_MASK		(BITSTREAM_BUFSIZE-1)

class BitReservoir
{
private:

	unsigned char		m_Buffer[BITSTREAM_BUFSIZE];

	long		m_InputIndex;

	long		m_ByteIndex;
	long		m_BitIndex;

public:

	BitReservoir()
	{
		FlushBuffer();
	}

	~BitReservoir()
	{
	}

	void FlushBuffer()
	{
		for(unsigned long x=0; x<BITSTREAM_BUFSIZE; x++)
		{
			m_Buffer[x] = 0xff;
		}

		m_InputIndex	= 0;
		m_ByteIndex		= 0;
		m_BitIndex		= 0;
	}

	void FillBitReserve(unsigned char * buffer, unsigned long numbytes)
	{
		m_ByteIndex = m_InputIndex;
		m_BitIndex	= 0;

		for(unsigned long x=0; x<numbytes; x++)
		{
			m_Buffer[m_InputIndex] = buffer[x];
			m_InputIndex	++;
			m_InputIndex	&= BITSTREAM_BUFSIZE_MASK;
		}
	}

	bool BackStep(unsigned long backstep)
	{
		if(backstep)
		{
			m_ByteIndex		-= backstep;
			m_ByteIndex		&= BITSTREAM_BUFSIZE_MASK;
		}

		return(true);
	}

	void RewindBits(int bits)
	{
		m_BitIndex -= bits;
	}

	void FlushBits(int bits)
	{
		m_BitIndex += bits;
	}

	unsigned long GetPos(void)
	{
		return(m_BitIndex);
	}

	void SetPos(unsigned long pos)
	{
		m_BitIndex = pos;
	}

	unsigned int ViewBits(int bits)
	{	
		unsigned long rval;
		int pos;

		if(!bits)
			return 0;

		pos = (unsigned long)m_ByteIndex + (m_BitIndex >> 3);

		rval =	m_Buffer[(pos)&0xFFF]	<< 24	|
				m_Buffer[(pos+1)&0xFFF]	<< 16	|
				m_Buffer[(pos+2)&0xFFF]	<< 8	|
				m_Buffer[(pos+3)&0xFFF];

		rval <<= m_BitIndex & 7;
		rval >>= 32-bits;

		return rval;
	}

	unsigned int GetBits(int bits)
	{	
		unsigned long rval;

		rval = ViewBits(bits);
		FlushBits(bits);

		return rval;
	}
};


