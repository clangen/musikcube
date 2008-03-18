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

#include "SideInfo.h"

SideInfo::SideInfo()
{

}

SideInfo::~SideInfo()
{

}

bool SideInfo::Load(Header * pHead, unsigned char *FromHere)
{
	unsigned long ch, gr, scfsi_band, region, window;

	bs.Load(FromHere);

	if(pHead->GetMpegVersion() == MPEG1)
	{
		MainDataBegin = bs.GetBits(9);

		if(pHead->GetMode() == MODE_MONO)
		{
			PrivateBits = bs.GetBits(5);
		}
		else
			PrivateBits = bs.GetBits(3);

		for(ch=0; ch<pHead->GetChannels(); ch++)
			for(scfsi_band=0; scfsi_band<4; scfsi_band++)
				ScfSi[ch][scfsi_band] = bs.GetBits(1);

		for(gr=0; gr<2; gr++)
		{
			for(ch=0; ch<pHead->GetChannels(); ch++)
			{
				grinf[gr][ch].Part23Length					= bs.GetBits(12);
				grinf[gr][ch].BigValues						= bs.GetBits(9);
				grinf[gr][ch].GlobalGain					= bs.GetBits(8);

				grinf[gr][ch].ScalefacCompress				= bs.GetBits(4);

				grinf[gr][ch].WindowSwitchingFlag			= bs.GetBits(1);
				
				if(grinf[gr][ch].WindowSwitchingFlag == 1)
				{
					grinf[gr][ch].BlockType					= bs.GetBits(2);
					if(grinf[gr][ch].BlockType == 0)
						return false;

					grinf[gr][ch].MixedBlockFlag			= bs.GetBits(1);

					grinf[gr][ch].TableSelect[0]			= bs.GetBits(5);
					grinf[gr][ch].TableSelect[1]			= bs.GetBits(5);
					grinf[gr][ch].TableSelect[2]			= 0;

					grinf[gr][ch].SubblockGain[0]			= bs.GetBits(3);
					grinf[gr][ch].SubblockGain[1]			= bs.GetBits(3);
					grinf[gr][ch].SubblockGain[2]			= bs.GetBits(3);

					// not used in short blocks
					if(	(grinf[gr][ch].BlockType == BLOCKTYPE_3WIN) && 
						(grinf[gr][ch].MixedBlockFlag == 0) )
					{
						grinf[gr][ch].Region0Count			= 8;
					}
					else
						grinf[gr][ch].Region0Count			= 7;

					grinf[gr][ch].Region1Count				= 20 - grinf[gr][ch].Region0Count;
				}
				else
				{
					grinf[gr][ch].BlockType					= 0;
					grinf[gr][ch].MixedBlockFlag			= 0;

					grinf[gr][ch].TableSelect[0]			= bs.GetBits(5);
					grinf[gr][ch].TableSelect[1]			= bs.GetBits(5);
					grinf[gr][ch].TableSelect[2]			= bs.GetBits(5);

					grinf[gr][ch].SubblockGain[0]			= 0;
					grinf[gr][ch].SubblockGain[1]			= 0;
					grinf[gr][ch].SubblockGain[2]			= 0;

					grinf[gr][ch].Region0Count				= bs.GetBits(4);
					grinf[gr][ch].Region1Count				= bs.GetBits(3);
				}

				grinf[gr][ch].PreFlag						= bs.GetBits(1);
				grinf[gr][ch].ScalefacScale					= bs.GetBits(1);
				grinf[gr][ch].Count1Table_Select			= bs.GetBits(1);
			}
		}
	}
	else
	{
		MainDataBegin = bs.GetBits(8);

		if(pHead->GetMode() == MODE_MONO)
			PrivateBits = bs.GetBits(1);
		else
			PrivateBits = bs.GetBits(2);

		gr=0;

		for(ch=0; ch<pHead->GetChannels(); ch++)
		{
			grinf[gr][ch].Part23Length					= bs.GetBits(12);
			grinf[gr][ch].BigValues						= bs.GetBits(9);
			grinf[gr][ch].GlobalGain					= bs.GetBits(8);

			grinf[gr][ch].ScalefacCompress				= bs.GetBits(9);

			grinf[gr][ch].WindowSwitchingFlag			= bs.GetBits(1);
			
			if(grinf[gr][ch].WindowSwitchingFlag == 1)
			{
				grinf[gr][ch].BlockType					= bs.GetBits(2);
				grinf[gr][ch].MixedBlockFlag			= bs.GetBits(1);

				for(region = 0; region < 2; region++)
				{
					grinf[gr][ch].TableSelect[region]	= bs.GetBits(5);
				}
				grinf[gr][ch].TableSelect[2]			= 0;

				for(window = 0; window < 3; window++)
				{
					grinf[gr][ch].SubblockGain[window]	= bs.GetBits(3);
				}

				// not used in short blocks
				if(	(grinf[gr][ch].BlockType == BLOCKTYPE_3WIN) && 
					(grinf[gr][ch].MixedBlockFlag == 0) )
				{
					grinf[gr][ch].Region0Count			= 8;
				}
				else
					grinf[gr][ch].Region0Count			= 7;

				grinf[gr][ch].Region1Count				= 20 - grinf[gr][ch].Region0Count;
			}
			else
			{
				grinf[gr][ch].BlockType					= 0;

				for(region = 0; region < 3; region++)
				{
					grinf[gr][ch].TableSelect[region]	= bs.GetBits(5);
				}

				grinf[gr][ch].Region0Count				= bs.GetBits(4);
				grinf[gr][ch].Region1Count				= bs.GetBits(3);
			}

			grinf[gr][ch].ScalefacScale					= bs.GetBits(1);
			grinf[gr][ch].Count1Table_Select			= bs.GetBits(1);
		}
	}

	return(true);
}
