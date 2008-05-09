// Header.cpp: implementation of the Header class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Header.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#define HDRMAIN_BITS	0xfffffc00			// 1111 1111 1111 1111 1111 1100 0000 0000
#define MPEG_BITS		0x00180000			// 0000 0000 0001 1000 0000 0000 0000 0000
#define LAYER_BITS		0x00060000			// 0000 0000 0000 0110 0000 0000 0000 0000
#define CRC_BIT			0x00010000			// 0000 0000 0000 0001 0000 0000 0000 0000
#define BITRATE_BITS	0x0000f000			// 0000 0000 0000 0000 ffff 0000 0000 0000
#define SAMP_BITS		0x00000C00			// 0000 0000 0000 0000 0000 1100 0000 0000 
#define PADING_BIT		0x00000200			// 0000 0000 0000 0000 0000 0010 0000 0000 
#define PRIVATE_BIT		0x00000100			// 0000 0000 0000 0000 0000 0001 0000 0000 
#define MODE_BITS		0x000000C0			// 0000 0000 0000 0000 0000 0000 1100 0000 
#define MODEEXT_BITS	0x00000030			// 0000 0000 0000 0000 0000 0000 0011 0000 
#define COPYRIGHT_BIT	0x00000008			// 0000 0000 0000 0000 0000 0000 0000 1000 
#define ORIGINAL_BIT	0x00000004			// 0000 0000 0000 0000 0000 0000 0000 0100 
#define EMPHASIS_BITS	0x00000003			// 0000 0000 0000 0000 0000 0000 0000 0011 

#define CHECK_BITS		0xfffe0c00

#define MPEG_BITS_1		0x00180000
#define MPEG_BITS_2		0x00100000
#define MPEG_BITS_25	0x00000000

#define LAYER_BITS_1	0x00060000
#define LAYER_BITS_2	0x00040000
#define LAYER_BITS_3	0x00020000


const unsigned long Header::BitRates[2][3][16] = 
{
	{
		{0 /*free format*/, 32000, 64000, 96000, 128000, 160000, 192000, 224000, 256000, 288000, 320000, 352000, 384000, 416000, 448000, 0},
		{0 /*free format*/, 32000, 48000, 56000,  64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000, 384000, 0},
		{0 /*free format*/, 32000, 40000, 48000,  56000,  64000,  80000,  96000, 112000, 128000, 160000, 192000, 224000, 256000, 320000, 0}
	},
	{
		{0 /*free format*/, 32000, 48000, 56000, 64000, 80000, 96000, 112000, 128000, 144000, 160000, 176000, 192000, 224000, 256000, 0},
		{0 /*free format*/,  8000, 16000, 24000, 32000, 40000, 48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000, 0},
		{0 /*free format*/,  8000, 16000, 24000, 32000, 40000, 48000,  56000,  64000,  80000,  96000, 112000, 128000, 144000, 160000, 0}
	},
};

const unsigned long Header::SampleRates[3][3] = 
{
	{
		44100, 48000, 32000
	},
	{
		22050, 24000, 16000
	},
	{
		11025, 12000, 8000
	}
};

Header::Header()
{
	Reset();
}

Header::~Header()
{

}

void Header::Reset()
{
	MpegVersion = -1;
	Layer = -1;
	SampleFrequency = -1;
	Mode = -1;
}

bool Header::Load(unsigned char *FromHere)
{
	bs.Load(FromHere);

	if(bs.GetBits(11) != 0x7ff)
		return(false);

	unsigned long OldMpeg = MpegVersion;
	if(bs.GetBits(1))			// mpeg 1 or 2
	{
		if(bs.GetBits(1))
		{	
			MpegVersion = MPEG1;
		}
		else
		{
			MpegVersion = MPEG2;
		}
	}
	else						// mpeg 2.5 or invalid
	{
		if(bs.GetBits(1))
		{
			MpegVersion = OldMpeg;
			return(false);
		}
		else
		{
			MpegVersion = MPEG25;
		}
	}

	if(OldMpeg != -1)
	{
		if(MpegVersion != OldMpeg)
		{
			MpegVersion = OldMpeg;
			return false;
		}
	}

	unsigned OldLayer = Layer;
	switch(bs.GetBits(2))
	{
		case 0:
			Layer		= OldLayer;
			return(false);
			break;

		case 1:
			Layer = LAYER3;
			break;

		case 2:
			Layer = LAYER2;
			break;

		case 3:
			Layer = LAYER1;
			break;
	}
	if(OldLayer != -1)
	{
		if(Layer != OldLayer)
		{
			MpegVersion = OldMpeg;
			Layer		= OldLayer;
			return false;
		}
	}

	ProtectionBit	= !(bs.GetBits(1));

	BitrateIndex	= bs.GetBits(4);
	if(BitrateIndex == 15)
	{
		MpegVersion = OldMpeg;
		Layer		= OldLayer;
		return(false);
	}

	if(BitrateIndex == 0)
	{
		MpegVersion = OldMpeg;
		Layer		= OldLayer;
		return(false);
	}

	unsigned long OldSampleFrequency = SampleFrequency;
	SampleFrequency = bs.GetBits(2);
	if(OldSampleFrequency != -1)
	{
		if(SampleFrequency != OldSampleFrequency)
		{
			MpegVersion		= OldMpeg;
			Layer			= OldLayer;
			SampleFrequency = OldSampleFrequency;
			return false;
		}
	}

	if(SampleFrequency == 3)
	{
		MpegVersion		= OldMpeg;
		Layer			= OldLayer;
		SampleFrequency = OldSampleFrequency;
		return false;
	}

	PaddingBit		= bs.GetBits(1);
	PrivateBit		= bs.GetBits(1);

	unsigned long OldMode = Mode;
	Mode			= bs.GetBits(2);
	if(OldMode != -1)
	{
		if(OldMode != Mode)
		{
			MpegVersion			= OldMpeg;
			Layer				= OldLayer;
			SampleFrequency		= OldSampleFrequency;
			Mode				= OldMode;
			return false;
		}
	}


	Mode_Extension	= bs.GetBits(2);

	Copyright		= bs.GetBits(1);
	Original		= bs.GetBits(1);

	Emphasis		= bs.GetBits(2);
	if(Emphasis == EMPH_RESERVED)
	{
			MpegVersion			= OldMpeg;
			Layer				= OldLayer;
			SampleFrequency		= OldSampleFrequency;
			Mode				= OldMode;

		return(false);
	}

	if(Mode == MODE_MONO)
		Channels = 1;
	else
		Channels = 2;


	if(Layer == LAYER3)
	{
		if(MpegVersion == MPEG1)
		{
			if(Channels==1)
				SideInfoSize = 17;
			else
				SideInfoSize = 32;
		}
		else
		{
			if(Channels==1)
				SideInfoSize = 9;
			else
				SideInfoSize = 17;
		}
	}
	else
		SideInfoSize = 0;

	unsigned long base;
	unsigned long Multiple;

	if(Layer == LAYER1)
	{
		base		= 12;
		Multiple	= 4;
	}
	else
	{
		// framesize is HALF of what it is for MPEG 1
		// really we should change that 144 * BitRate...
		// to be 72 * BitRate....
		if(MpegVersion != MPEG1)
			base = 72;
		else
			base = 144;

		Multiple = 1;
	}

	FrameSize	= ((base * BitRates[MpegVersion == MPEG1 ? 0 : 1][Layer][BitrateIndex]) / SampleRates[MpegVersion][SampleFrequency]);
	if(PaddingBit)
		FrameSize++;

	FrameSize  *= Multiple;
	FrameSize -= 4; // for the Header;
	if(ProtectionBit)
		FrameSize -= 2;
	FrameSize -= SideInfoSize;

	HeaderSize = 4;

	return(true);
}
