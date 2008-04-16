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

#include "BitStream.h"	// Added by ClassView
#include "Header.h"

/* used in Granule.block_type */
#define BLOCKTYPE_NORMAL	0
#define BLOCKTYPE_START		1
#define BLOCKTYPE_3WIN		2
#define BLOCKTYPE_STOP		3

#define MAXCH				2
#define MAXGR				2
#define MAXSCFCI			4

class SideInfo  
{
protected:
	struct GranuleInfo
	{
		unsigned int	Part23Length;	// number of bits used for scalefactors and huffmancode data
		unsigned int	BigValues;
		unsigned int	GlobalGain;

		unsigned int	ScalefacCompress;
		unsigned int	WindowSwitchingFlag;

		unsigned int	BlockType;
		unsigned int	MixedBlockFlag;

		unsigned int	TableSelect[4];
		unsigned int	SubblockGain[4];

		unsigned int	Region0Count;
		unsigned int	Region1Count;

		unsigned int	PreFlag;

		unsigned int	ScalefacScale;
		unsigned int	Count1Table_Select;

		unsigned int	Scalefac_Long[23];     /* actual scalefactors for this granule */
		unsigned int	Scalefac_Short[13][3];  /* scalefactors used in short windows */

		unsigned int	IntensityScale;
		unsigned int	is_max[21];
	};

	BitStream	bs;

public:
	SideInfo();
	virtual ~SideInfo();

	bool Load(Header * pHead, unsigned char * FromHere);

public:
	unsigned int	MainDataBegin;
	unsigned int	PrivateBits;

	unsigned int	ScfSi[MAXCH][MAXSCFCI];

	GranuleInfo		grinf[MAXGR][MAXCH];
};
