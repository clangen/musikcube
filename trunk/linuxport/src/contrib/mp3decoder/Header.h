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

#define LAYER1				0
#define LAYER2				1
#define LAYER3				2

#define MPEG1				0
#define MPEG2				1
#define MPEG25				2

#define MODE_STEREO			0
#define MODE_JOINT_STEREO	1	// see MODE_EXT defines
#define MODE_DUAL_CHANNEL	2
#define MODE_MONO			3

//mode extensions

// for LAYER1 and LAYER2
#define BOUND4				0
#define BOUND8				1
#define BOUND12				2
#define BOUND16				3

// for LAYER3
#define MODE_EXT_STEREO		0	// stream is actually MODE_STEREO
#define MODE_EXT_IS			1	// Intensity stereo coding is used
#define MODE_EXT_MS			2	// MS Stereo is used
#define MODE_EXT_ISMS		3	// both Intensity and MS stereo are used

#define EMPH_NONE			0	// no deemphasis
#define EMPH_5015			1	// 50/15 microseconds
#define EMPH_RESERVED		2	// should never happen
#define EMPH_CCITT			3	// CCITT j.17

class Header  
{
protected:
	// Mpeg Header Stuff
	unsigned long	MpegVersion;
	unsigned long	Layer;
	unsigned long	ProtectionBit;
	unsigned long	BitrateIndex;
	unsigned long	SampleFrequency;
	unsigned long	PaddingBit;
	unsigned long	PrivateBit;
	unsigned long	Mode;
	unsigned long	Mode_Extension;
	unsigned long	Copyright;
	unsigned long	Original;
	unsigned long	Emphasis;

	// Calculated stuff
	unsigned long	FrameSize;
	unsigned long	SideInfoSize;
	unsigned long	Channels;

	unsigned long	HeaderSize;

	unsigned char	CRC[2];

	static const unsigned long BitRates[2][3][16];
	static const unsigned long SampleRates[3][3];

	BitStream		bs;

public:
	Header();
	virtual ~Header();

	void Reset();

	bool Load(unsigned char * FromHere);

	unsigned long GetHeaderSize(void)		{ return(HeaderSize); };
	unsigned long GetCRCSize()				{ if(IsCRC()) return 2; else return 0; };
	unsigned long GetSideInfoSize(void)		{ return(SideInfoSize); }
	unsigned long GetDataSize(void)			{ return(FrameSize); }

	unsigned long GetMpegVersion()			{ return MpegVersion; };
	unsigned long GetMode()					{ return Mode; };
	unsigned long GetModeExtension()		{ return Mode_Extension; };
	unsigned long GetLayer()				{ return Layer; };
	unsigned long GetChannels()				{ return Channels; };
	unsigned long GetSampleFrequencyIndex()	{ return SampleFrequency; };
	unsigned long GetSampleFrequency()		{ return SampleRates[MpegVersion][SampleFrequency]; };
	unsigned long IsCRC() 					{ return( ProtectionBit ); };

	unsigned long GetTotalFrameSize()		{ return( GetHeaderSize() + GetCRCSize() + GetSideInfoSize() + GetDataSize() ); }

	unsigned long GetBitrate()				{ return( BitRates[MpegVersion == MPEG1 ? 0 : 1][Layer][BitrateIndex] ); };
};
