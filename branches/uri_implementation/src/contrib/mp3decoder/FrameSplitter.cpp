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

#include "stdafx.h"
#include "FrameSplitter.h"

CFrameSplitter::CFrameSplitter()
{

}

CFrameSplitter::~CFrameSplitter()
{

}

bool CFrameSplitter::Process(HANDLE hRead, Frame & fr)
{
	unsigned char	headerbuf[4] = {0,0,0,0};
	unsigned char	crcbuf[2];
	unsigned char	sideInfoBuffer[32];
	unsigned long	BytesRead;
	unsigned long	NumErrors = 0;
	unsigned long	CurrentOffset = SetFilePointer(hRead, 0, NULL, FILE_CURRENT);

	if(!ReadFile(hRead, (char*)headerbuf, 4, &BytesRead, NULL))
		return false;
	if(BytesRead != 4)
		return false;

	while(!fr.m_Header.Load(headerbuf))
	{
		headerbuf[0] = headerbuf[1];
		headerbuf[1] = headerbuf[2];
		headerbuf[2] = headerbuf[3];

		if(!ReadFile(hRead, (char*)&headerbuf[3], 1, &BytesRead, NULL))
			return false;
		if(BytesRead != 1)
			return false;

		if(NumErrors++ >= fr.m_Header.GetDataSize())
		{
			fr.m_Header.Reset();
		}

		CurrentOffset++;
	}

	// check we have enough for the whole of this frame...
	//if(BytesUsed <= ((int)Length - (int)FrameSize))

	if(fr.m_Header.IsCRC())
	{
		if(!ReadFile(hRead, (char*)crcbuf, 2, &BytesRead, NULL))
			return false;
		if(BytesRead != 2)
			return false;

		fr.m_CRC.Load(crcbuf);
	}

	
	if(fr.m_Header.GetLayer() == LAYER3)
	{
		// read side info for layer 3 files

		if(!ReadFile(hRead, (char*)sideInfoBuffer, fr.m_Header.GetSideInfoSize(), &BytesRead, NULL))
			return false;
		if(BytesRead != fr.m_Header.GetSideInfoSize())
			return false;

		if(!fr.m_SI.Load(&fr.m_Header, sideInfoBuffer))
			return(false);

	}

	if(!ReadFile(hRead, (char*)fr.m_Data, fr.m_Header.GetDataSize(), &BytesRead, NULL))
		return false;
	if(BytesRead != fr.m_Header.GetDataSize())
		return false;

	return(true);
}
