#pragma once

#include "BaseDecoder.h"
#include "BitReservoir.h"
#include "SideInfo.h"

class CLayer3Decoder : public CBaseDecoder  
{
protected:

	typedef struct _SBI_
	{
		unsigned short Long[23];
		unsigned short Short[14];
	} SBI;


	BitReservoir	br;
	Frame *			m_fr;


	unsigned long	m_Part2Start[2];
	unsigned long	m_MixedBandLimit[2];
	unsigned long	m_NonZero[2];

	unsigned long	m_MpegVer;
	unsigned long	m_SampleFrequency;
	unsigned long	m_Channels;
	unsigned long	m_Granules;
	unsigned long	m_Mode;
	unsigned long	m_ModeExt;

	int				is[2][576];
	float			xr[2][576];	// Dequantized
	float			xrr[2][576];	// reordered
	float			xir[2][576];	// IMDCT'd

	float				kl[576];
	float				kr[576];
	float				prevblck[2][576];	// overlapped
	float				IMDCTwin[4][36];	// used for windowing the IMDCT samples


	// calculated at runtime
	float PowerTableMinus2[64]; /* (2^(-2))^i */
	float PowerTableMinus05[64]; /* (2^(-0.5))^i */
	float GainTable[256];
	float TanPi12Table[16];	// mpeg1 tan(is * PI/12);
	float Cs[8], Ca[8];
	float IMDCT9x8Table[72];

	static const SBI sfBandIndex[3][3];
	static const float ShortTwiddles[];
	static const float NormalTwiddles[];

	void DecodeScalefactors(unsigned long ch, unsigned long gr);

	bool HuffmanDecode(unsigned long TableNum, int * x, int * y, int * v, int * w);
	bool ReadHuffman(unsigned long ch, unsigned long gr);
	void DequantizeSample(int ch, int gr);
	void Reorder(unsigned int ch, unsigned int gr);
	void CalculateK(int index, int is_pos, int intensity_scale);
	void Stereo(unsigned int gr);
	void AntiAlias(unsigned int ch, unsigned int gr);
	void FreqencyInverse(int gr, int ch);

	void IMDCT(float *in, float *out, int block_type);
	void Hybrid(int ch, float *xfrom, float *xto, int blocktype, int windowswitching, int mixedblock);

public:
	CLayer3Decoder();
	virtual ~CLayer3Decoder();

public:
	bool ProcessFrame(Frame * fr, float * PCMSamples, unsigned long * NumSamples);

};
