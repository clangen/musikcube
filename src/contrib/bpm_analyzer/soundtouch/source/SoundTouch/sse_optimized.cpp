////////////////////////////////////////////////////////////////////////////////
///
/// SSE optimized routines for Pentium-III, Athlon-XP and later CPUs. All SSE 
/// optimized functions have been gathered into this single source 
/// code file, regardless to their class or original source code file, in order 
/// to ease porting the library to other compiler and processor platforms.
///
/// The SSE-optimizations are programmed using SSE compiler intrinsics that
/// are supported both by Microsoft Visual C++ and GCC compilers, so this file
/// should compile with both toolsets.
///
/// NOTICE: If using Visual Studio 6.0, you'll need to install the "Visual C++ 
/// 6.0 processor pack" update to support SSE instruction set. The update is 
/// available for download at Microsoft Developers Network, see here:
/// http://msdn.microsoft.com/vstudio/downloads/tools/ppack/default.aspx
///
/// If the above URL is expired or removed, go to "http://msdn.microsoft.com" and 
/// perform a search with keywords "processor pack".
///
/// Author        : Copyright (c) Olli Parviainen
/// Author e-mail : oparviai 'at' iki.fi
/// SoundTouch WWW: http://www.surina.net/soundtouch
///
////////////////////////////////////////////////////////////////////////////////
//
// Last changed  : $Date: 2006/02/05 16:44:06 $
// File revision : $Revision: 1.2 $
//
// $Id: sse_optimized.cpp,v 1.2 2006/02/05 16:44:06 Olli Exp $
//
////////////////////////////////////////////////////////////////////////////////
//
// License :
//
//  SoundTouch audio processing library
//  Copyright (c) Olli Parviainen
//
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
////////////////////////////////////////////////////////////////////////////////

#include "cpu_detect.h"
#include "STTypes.h"

using namespace soundtouch;

#ifdef ALLOW_SSE

// SSE routines available only with float sample type    

//////////////////////////////////////////////////////////////////////////////
//
// implementation of SSE optimized functions of class 'TDStretchSSE'
//
//////////////////////////////////////////////////////////////////////////////

#include "TDStretch.h"
#include <xmmintrin.h>

// Calculates cross correlation of two buffers
double TDStretchSSE::calcCrossCorrStereo(const float *pV1, const float *pV2) const
{
    uint i;
    __m128 vSum, *pVec2;

    // Note. It means a major slow-down if the routine needs to tolerate 
    // unaligned __m128 memory accesses. It's way faster if we can skip 
    // unaligned slots and use _mm_load_ps instruction instead of _mm_loadu_ps.
    // This can mean up to ~ 10-fold difference (incl. part of which is
    // due to skipping every second round for stereo sound though).
    //
    // Compile-time define ALLOW_NONEXACT_SIMD_OPTIMIZATION is provided
    // for choosing if this little cheating is allowed.

#ifdef ALLOW_NONEXACT_SIMD_OPTIMIZATION
    // Little cheating allowed, return valid correlation only for 
    // aligned locations, meaning every second round for stereo sound.

    #define _MM_LOAD    _mm_load_ps

    if (((ulong)pV1) & 15) return -1e50;    // skip unaligned locations

#else
    // No cheating allowed, use unaligned load & take the resulting
    // performance hit.
    #define _MM_LOAD    _mm_loadu_ps
#endif 

    // ensure overlapLength is divisible by 8
    assert((overlapLength % 8) == 0);

    // Calculates the cross-correlation value between 'pV1' and 'pV2' vectors
    // Note: pV2 _must_ be aligned to 16-bit boundary, pV1 need not.
    pVec2 = (__m128*)pV2;
    vSum = _mm_setzero_ps();

    // Unroll the loop by factor of 4 * 4 operations
    for (i = 0; i < overlapLength / 8; i ++) 
    {
        // vSum += pV1[0..3] * pV2[0..3]
        vSum = _mm_add_ps(vSum, _mm_mul_ps(_MM_LOAD(pV1),pVec2[0]));

        // vSum += pV1[4..7] * pV2[4..7]
        vSum = _mm_add_ps(vSum, _mm_mul_ps(_MM_LOAD(pV1 + 4), pVec2[1]));

        // vSum += pV1[8..11] * pV2[8..11]
        vSum = _mm_add_ps(vSum, _mm_mul_ps(_MM_LOAD(pV1 + 8), pVec2[2]));

        // vSum += pV1[12..15] * pV2[12..15]
        vSum = _mm_add_ps(vSum, _mm_mul_ps(_MM_LOAD(pV1 + 12), pVec2[3]));

        pV1 += 16;
        pVec2 += 4;
    }

    // return value = vSum[0] + vSum[1] + vSum[2] + vSum[3]
    float *pvSum = (float*)&vSum;
    return (double)(pvSum[0] + pvSum[1] + pvSum[2] + pvSum[3]);

    /* This is approximately corresponding routine in C-language:
    double corr;
    uint i;

    // Calculates the cross-correlation value between 'pV1' and 'pV2' vectors
    corr = 0.0;
    for (i = 0; i < overlapLength / 8; i ++) 
    {
        corr += pV1[0] * pV2[0] +
                pV1[1] * pV2[1] +
                pV1[2] * pV2[2] +
                pV1[3] * pV2[3] +
                pV1[4] * pV2[4] +
                pV1[5] * pV2[5] +
                pV1[6] * pV2[6] +
                pV1[7] * pV2[7] +
                pV1[8] * pV2[8] +
                pV1[9] * pV2[9] +
                pV1[10] * pV2[10] +
                pV1[11] * pV2[11] +
                pV1[12] * pV2[12] +
                pV1[13] * pV2[13] +
                pV1[14] * pV2[14] +
                pV1[15] * pV2[15];

        pV1 += 16;
        pV2 += 16;
    }
    */

    /* This is corresponding routine in assembler. This may be teeny-weeny bit faster
       than intrinsic version, but more difficult to maintain & get compiled on multiple
       platforms.

    uint overlapLengthLocal = overlapLength;
    float corr;

    _asm 
    {
        // Very important note: data in 'pV2' _must_ be aligned to 
        // 16-byte boundary!

        // give prefetch hints to CPU of what data are to be needed soonish
        // give more aggressive hints on pV1 as that changes while pV2 stays
        // same between runs
        prefetcht0 [pV1]
        prefetcht0 [pV2]
        prefetcht0 [pV1 + 32]

        mov     eax, dword ptr pV1
        mov     ebx, dword ptr pV2

        xorps   xmm0, xmm0

        mov     ecx, overlapLengthLocal
        shr     ecx, 3  // div by eight

    loop1:
        prefetcht0 [eax + 64]     // give a prefetch hint to CPU what data are to be needed soonish
        prefetcht0 [ebx + 32]     // give a prefetch hint to CPU what data are to be needed soonish
        movups  xmm1, [eax]
        mulps   xmm1, [ebx]
        addps   xmm0, xmm1

        movups  xmm2, [eax + 16]
        mulps   xmm2, [ebx + 16]
        addps   xmm0, xmm2

        prefetcht0 [eax + 96]     // give a prefetch hint to CPU what data are to be needed soonish
        prefetcht0 [ebx + 64]     // give a prefetch hint to CPU what data are to be needed soonish

        movups  xmm3, [eax + 32]
        mulps   xmm3, [ebx + 32]
        addps   xmm0, xmm3

        movups  xmm4, [eax + 48]
        mulps   xmm4, [ebx + 48]
        addps   xmm0, xmm4

        add     eax, 64
        add     ebx, 64

        dec     ecx
        jnz     loop1

        // add the four floats of xmm0 together and return the result. 

        movhlps xmm1, xmm0          // move 3 & 4 of xmm0 to 1 & 2 of xmm1
        addps   xmm1, xmm0
        movaps  xmm2, xmm1
        shufps  xmm2, xmm2, 0x01    // move 2 of xmm2 as 1 of xmm2
        addss   xmm2, xmm1
        movss   corr, xmm2
    }

    return (double)corr;
    */
}


//////////////////////////////////////////////////////////////////////////////
//
// implementation of SSE optimized functions of class 'FIRFilter'
//
//////////////////////////////////////////////////////////////////////////////

#include "FIRFilter.h"

FIRFilterSSE::FIRFilterSSE() : FIRFilter()
{
    filterCoeffsUnalign = NULL;
}


FIRFilterSSE::~FIRFilterSSE()
{
    delete[] filterCoeffsUnalign;
}


// (overloaded) Calculates filter coefficients for SSE routine
void FIRFilterSSE::setCoefficients(const float *coeffs, uint newLength, uint uResultDivFactor)
{
    uint i;
    float fDivider;

    FIRFilter::setCoefficients(coeffs, newLength, uResultDivFactor);

    // Scale the filter coefficients so that it won't be necessary to scale the filtering result
    // also rearrange coefficients suitably for 3DNow!
    // Ensure that filter coeffs array is aligned to 16-byte boundary
    delete[] filterCoeffsUnalign;
    filterCoeffsUnalign = new float[2 * newLength + 4];
    filterCoeffsAlign = (float *)(((unsigned long)filterCoeffsUnalign + 15) & -16);

    fDivider = (float)resultDivider;

    // rearrange the filter coefficients for mmx routines 
    for (i = 0; i < newLength; i ++)
    {
        filterCoeffsAlign[2 * i + 0] =
        filterCoeffsAlign[2 * i + 1] = coeffs[i + 0] / fDivider;
    }
}



// SSE-optimized version of the filter routine for stereo sound
uint FIRFilterSSE::evaluateFilterStereo(float *dest, const float *source, uint numSamples) const
{
    int count = (numSamples - length) & -2;
    int j;

    assert(count % 2 == 0);

    if (count < 2) return 0;

    assert((length % 8) == 0);
    assert(((unsigned long)filterCoeffsAlign) % 16 == 0);

    // filter is evaluated for two stereo samples with each iteration, thus use of 'j += 2'
    for (j = 0; j < count; j += 2)
    {
        const float *pSrc;
        const __m128 *pFil;
        __m128 sum1, sum2;
        uint i;

        pSrc = source;                      // source audio data
        pFil = (__m128*)filterCoeffsAlign;  // filter coefficients. NOTE: Assumes coefficients 
                                            // are aligned to 16-byte boundary
        sum1 = sum2 = _mm_setzero_ps();

        for (i = 0; i < length / 8; i ++) 
        {
            // Unroll loop for efficiency & calculate filter for 2*2 stereo samples 
            // at each pass

            // sum1 is accu for 2*2 filtered stereo sound data at the primary sound data offset
            // sum2 is accu for 2*2 filtered stereo sound data for the next sound sample offset.

            sum1 = _mm_add_ps(sum1, _mm_mul_ps(_mm_loadu_ps(pSrc)    , pFil[0]));
            sum2 = _mm_add_ps(sum2, _mm_mul_ps(_mm_loadu_ps(pSrc + 2), pFil[0]));

            sum1 = _mm_add_ps(sum1, _mm_mul_ps(_mm_loadu_ps(pSrc + 4), pFil[1]));
            sum2 = _mm_add_ps(sum2, _mm_mul_ps(_mm_loadu_ps(pSrc + 6), pFil[1]));

            sum1 = _mm_add_ps(sum1, _mm_mul_ps(_mm_loadu_ps(pSrc + 8) ,  pFil[2]));
            sum2 = _mm_add_ps(sum2, _mm_mul_ps(_mm_loadu_ps(pSrc + 10), pFil[2]));

            sum1 = _mm_add_ps(sum1, _mm_mul_ps(_mm_loadu_ps(pSrc + 12), pFil[3]));
            sum2 = _mm_add_ps(sum2, _mm_mul_ps(_mm_loadu_ps(pSrc + 14), pFil[3]));

            pSrc += 16;
            pFil += 4;
        }

        // Now sum1 and sum2 both have a filtered 2-channel sample each, but we still need
        // to sum the two hi- and lo-floats of these registers together.

        // post-shuffle & add the filtered values and store to dest.
        _mm_storeu_ps(dest, _mm_add_ps(
                    _mm_shuffle_ps(sum1, sum2, _MM_SHUFFLE(1,0,3,2)),   // s2_1 s2_0 s1_3 s1_2
                    _mm_shuffle_ps(sum1, sum2, _MM_SHUFFLE(3,2,1,0))    // s2_3 s2_2 s1_1 s1_0
                    ));
        source += 4;
        dest += 4;
    }

    // Ideas for further improvement:
    // 1. If it could be guaranteed that 'source' were always aligned to 16-byte 
    //    boundary, a faster aligned '_mm_load_ps' instruction could be used.
    // 2. If it could be guaranteed that 'dest' were always aligned to 16-byte 
    //    boundary, a faster '_mm_store_ps' instruction could be used.

    return (uint)count;

    /* original routine in C-language. please notice the C-version has differently 
       organized coefficients though.
    double suml1, suml2;
    double sumr1, sumr2;
    uint i, j;

    for (j = 0; j < count; j += 2)
    {
        const float *ptr;
        const float *pFil;

        suml1 = sumr1 = 0.0;
        suml2 = sumr2 = 0.0;
        ptr = src;
        pFil = filterCoeffs;
        for (i = 0; i < lengthLocal; i ++) 
        {
            // unroll loop for efficiency.

            suml1 += ptr[0] * pFil[0] + 
                     ptr[2] * pFil[2] +
                     ptr[4] * pFil[4] +
                     ptr[6] * pFil[6];

            sumr1 += ptr[1] * pFil[1] + 
                     ptr[3] * pFil[3] +
                     ptr[5] * pFil[5] +
                     ptr[7] * pFil[7];

            suml2 += ptr[8] * pFil[0] + 
                     ptr[10] * pFil[2] +
                     ptr[12] * pFil[4] +
                     ptr[14] * pFil[6];

            sumr2 += ptr[9] * pFil[1] + 
                     ptr[11] * pFil[3] +
                     ptr[13] * pFil[5] +
                     ptr[15] * pFil[7];

            ptr += 16;
            pFil += 8;
        }
        dest[0] = (float)suml1;
        dest[1] = (float)sumr1;
        dest[2] = (float)suml2;
        dest[3] = (float)sumr2;

        src += 4;
        dest += 4;
    }
    */


    /* Similar routine in assembly, again obsoleted due to maintainability
    _asm
    {
        // Very important note: data in 'src' _must_ be aligned to 
        // 16-byte boundary!
        mov     edx, count
        mov     ebx, dword ptr src
        mov     eax, dword ptr dest
        shr     edx, 1

    loop1:
        // "outer loop" : during each round 2*2 output samples are calculated

        // give prefetch hints to CPU of what data are to be needed soonish
        prefetcht0 [ebx]
        prefetcht0 [filterCoeffsLocal]

        mov     esi, ebx
        mov     edi, filterCoeffsLocal
        xorps   xmm0, xmm0
        xorps   xmm1, xmm1
        mov     ecx, lengthLocal

    loop2:
        // "inner loop" : during each round eight FIR filter taps are evaluated for 2*2 samples
        prefetcht0 [esi + 32]     // give a prefetch hint to CPU what data are to be needed soonish
        prefetcht0 [edi + 32]     // give a prefetch hint to CPU what data are to be needed soonish

        movups  xmm2, [esi]         // possibly unaligned load
        movups  xmm3, [esi + 8]     // possibly unaligned load
        mulps   xmm2, [edi]
        mulps   xmm3, [edi]
        addps   xmm0, xmm2
        addps   xmm1, xmm3

        movups  xmm4, [esi + 16]    // possibly unaligned load
        movups  xmm5, [esi + 24]    // possibly unaligned load
        mulps   xmm4, [edi + 16]
        mulps   xmm5, [edi + 16]
        addps   xmm0, xmm4
        addps   xmm1, xmm5

        prefetcht0 [esi + 64]     // give a prefetch hint to CPU what data are to be needed soonish
        prefetcht0 [edi + 64]     // give a prefetch hint to CPU what data are to be needed soonish

        movups  xmm6, [esi + 32]    // possibly unaligned load
        movups  xmm7, [esi + 40]    // possibly unaligned load
        mulps   xmm6, [edi + 32]
        mulps   xmm7, [edi + 32]
        addps   xmm0, xmm6
        addps   xmm1, xmm7

        movups  xmm4, [esi + 48]    // possibly unaligned load
        movups  xmm5, [esi + 56]    // possibly unaligned load
        mulps   xmm4, [edi + 48]
        mulps   xmm5, [edi + 48]
        addps   xmm0, xmm4
        addps   xmm1, xmm5

        add     esi, 64
        add     edi, 64
        dec     ecx
        jnz     loop2

        // Now xmm0 and xmm1 both have a filtered 2-channel sample each, but we still need
        // to sum the two hi- and lo-floats of these registers together.

        movhlps xmm2, xmm0          // xmm2 = xmm2_3 xmm2_2 xmm0_3 xmm0_2
        movlhps xmm2, xmm1          // xmm2 = xmm1_1 xmm1_0 xmm0_3 xmm0_2
        shufps  xmm0, xmm1, 0xe4    // xmm0 = xmm1_3 xmm1_2 xmm0_1 xmm0_0
        addps   xmm0, xmm2

        movaps  [eax], xmm0
        add     ebx, 16
        add     eax, 16

        dec     edx
        jnz     loop1
    }
    */
}

#endif  // ALLOW_SSE
