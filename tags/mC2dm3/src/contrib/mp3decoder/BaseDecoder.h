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

#include "IMPEGDecoder.h"

#define PI		(3.1415926535897932384626433832795f)
#define PI12	(0.2617993877991494365385536152732f)
#define PI18	(0.17453292519943295769236907684886f)
#define PI36	(0.0872664625997164788461845384244f)

#define SQRT2	(1.4142135623730950488016887242097f)


class CBaseDecoder : public IMPEGDecoder  
{
protected:
	float A16[16][16], A8[8][8];       /* DCT matrix         */
	float G16[16][16], G8[8][8];       /* Output butterfly   */
	float H16[16][16], H8[8][8];       /* Scaling            */
	float B16[16][16], B8[8][8];       /* B = G * DCT * H    */

	float DTable[512];

	float V[2][1024];
	int Vpointer[2];

	static const float D[512];

	void MultiplyMatrix16(float in1[16][16], float in2[16][16], float out[16][16])
	{
		int i,j,z;

		for(i = 0; i < 16; i++) 
		{
			for(j = 0; j < 16; j++) 
			{
				out[i][j] = 0.0;
				for(z = 0; z < 16; z++)
					out[i][j] += in1[i][z] * in2[z][j];
			}
		}
	}

	void MultiplyMatrix8(float in1[8][8], float in2[8][8], float out[8][8])
	{
		int i,j,z;

		for(i = 0; i < 8; i++) 
		{
			for(j = 0; j < 8; j++) 
			{
				out[i][j] = 0.0;
				for(z = 0; z < 8; z++)
					out[i][j] += in1[i][z] * in2[z][j];
			}
		}
	}

	void IDCT(float *in, float *out);
	void Window(int ch, float *S, int step);
	bool PerformSynthesis(float *InputFreq, float *ToHere, int Channel, int step);

public:
	CBaseDecoder();
	virtual ~CBaseDecoder();

};
