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

#include "BitStream.h"


BitStream::BitStream()
{
	buffer		= 0;
	bitindex	= 0;
}

BitStream::~BitStream()
{

}

unsigned long BitStream::GetBits(unsigned long N)
{
	unsigned long rval;
	int pos;

	if(N == 0)
		return 0;

	pos = (unsigned long)(bitindex >> 3);

	rval =	buffer[pos]		<< 24 |
			buffer[pos+1]	<< 16 |
			buffer[pos+2]	<< 8 |
			buffer[pos+3];

	rval <<= bitindex & 7;
	rval >>= 32 - N;
	bitindex += N;

	return rval;
}


bool BitStream::Load(unsigned char *FromHere)
{
	buffer		= FromHere;
	bitindex	= 0;

	return(true);
}
