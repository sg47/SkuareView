/*****************************************************************************/
// File: avx_coder_local.cpp [scope = CORESYS/CODING]
// Version: Kakadu, V7.2
// Author: David Taubman
// Last Revised: 17 January, 2013
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: International Centre For Radio Astronomy Research, Uni of WA
// License number: 01265
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
Description:
   Provides SIMD implementations to accelerate the conversion and transfer of
data between the block coder and DWT line-based processing engine.  This
source file is required to implement AVX versions of the data transfer
functions -- placing the code in a separate file allows the compiler to
be instructed to use vex-prefixed instructions exclusively, which avoids
processor state transition costs.  There is no harm in including this
source file with all builds, even if AVX is not supported, so long as you
are careful to globally define the `KDU_NO_AVX' compilation directive.
******************************************************************************/
#if ((!defined KDU_NO_AVX) && (defined KDU_X86_INTRINSICS))

#ifdef _MSC_VER
#  include <intrin.h>
#else
#  include <immintrin.h>
#endif // !_MSC_VER
#include <assert.h>

typedef short int kdu_int16;
typedef unsigned short int kdu_uint16;
typedef int kdu_int32;
typedef unsigned int kdu_uint32;

#define KDU_FIX_POINT ((int) 13)

/* ========================================================================= */
/*                         Now for the SIMD functions                        */
/* ========================================================================= */

/*****************************************************************************/
/* EXTERN                   avx_irrev_decoded_block16                        */
/*****************************************************************************/

void
  avx_xfer_irrev_decoded_block16(kdu_int32 *src, kdu_int16 **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max, float delta)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  float fscale = delta * (float)(1<<KDU_FIX_POINT);
  if (K_max <= 31)
    fscale /= (float)(1<<(31-K_max));
  else
    fscale *= (float)(1<<(K_max-31));
  int mxcsr_orig = _mm_getcsr();
  int mxcsr_cur = mxcsr_orig & ~(3<<13); // Reset rounding control bits
  _mm_setcsr(mxcsr_cur);
  __m256 vec_scale = _mm256_set1_ps(fscale);
  __m256 smask=_mm256_castsi256_ps(_mm256_set1_epi32(0x80000000));
  __m256 *sp=(__m256 *)src;
  if (!(width & 31))
    for (; height > 0; height--, dst_refs++)
      { // Common case: process in groups of 32 samples at a time
        __m128i *dp = (__m128i *)(*dst_refs + dst_offset);
        __m256 v1, v2, v3, v4, s1, s2, s3, s4;
        __m256i iv1, iv2, iv3, iv4;
        __m128i e1, e2, e3, e4;
        for (int c=width; c > 0; c-=32, dp+=4, sp+=4)
          { 
            v1=sp[0];  v2=sp[1];  v3=sp[2];  v4=sp[3];
            s1 = _mm256_and_ps(v1,smask);     s2 = _mm256_and_ps(v2,smask);
            s3 = _mm256_and_ps(v3,smask);     s4 = _mm256_and_ps(v4,smask);
            v1 = _mm256_andnot_ps(smask,v1);  v2 = _mm256_andnot_ps(smask,v2);
            v3 = _mm256_andnot_ps(smask,v3);  v4 = _mm256_andnot_ps(smask,v4);
            v1 = _mm256_cvtepi32_ps(_mm256_castps_si256(v1));
            v2 = _mm256_cvtepi32_ps(_mm256_castps_si256(v2));
            v3 = _mm256_cvtepi32_ps(_mm256_castps_si256(v3));
            v4 = _mm256_cvtepi32_ps(_mm256_castps_si256(v4));
            v1 = _mm256_mul_ps(v1,vec_scale); v2 = _mm256_mul_ps(v2,vec_scale);
            v3 = _mm256_mul_ps(v3,vec_scale); v4 = _mm256_mul_ps(v4,vec_scale);
            v1 = _mm256_or_ps(v1,s1);         v2 = _mm256_or_ps(v2,s2);
            v3 = _mm256_or_ps(v3,s3);         v4 = _mm256_or_ps(v4,s4);
            iv1 = _mm256_cvtps_epi32(v1);     iv2 = _mm256_cvtps_epi32(v2);
            iv3 = _mm256_cvtps_epi32(v3);     iv4 = _mm256_cvtps_epi32(v4);
            e1 = _mm256_extractf128_si256(iv1,1);
            e2 = _mm256_extractf128_si256(iv2,1);
            e3 = _mm256_extractf128_si256(iv3,1);
            e4 = _mm256_extractf128_si256(iv4,1);
            e1 = _mm_packs_epi32(_mm256_castsi256_si128(iv1),e1);
            e2 = _mm_packs_epi32(_mm256_castsi256_si128(iv2),e2);
            e3 = _mm_packs_epi32(_mm256_castsi256_si128(iv3),e3);
            e4 = _mm_packs_epi32(_mm256_castsi256_si128(iv4),e4);
            _mm_stream_ps((float *) dp,    _mm_castsi128_ps(e1));
            _mm_stream_ps((float *)(dp+1), _mm_castsi128_ps(e2));
            _mm_stream_ps((float *)(dp+2), _mm_castsi128_ps(e3));
            _mm_stream_ps((float *)(dp+3), _mm_castsi128_ps(e4));
            //dp[0]=e1;  dp[1]=e2;  dp[2]=e3;  dp[3]=e4;
          }
      }
  else
    for (; height > 0; height--, dst_refs++)
      { // Process individual octets
        __m128i *dp = (__m128i *)(*dst_refs + dst_offset);
        for (int c=width; c > 0; c -= 8, dp++, sp++)
          { 
            __m256 ival = sp[0];
            __m256 sval = _mm256_and_ps(ival,smask); // Save sign bits
            __m256 mval = _mm256_andnot_ps(smask,ival); // Remove sign
            __m256 fval = _mm256_cvtepi32_ps(_mm256_castps_si256(mval));
            fval = _mm256_mul_ps(fval,vec_scale);
            fval = _mm256_or_ps(fval,sval); // Put sign bits back
            __m256i cval = _mm256_cvtps_epi32(fval);
            __m128i val1 = _mm256_castsi256_si128(cval);
            __m128i val2 = _mm256_extractf128_si256(cval,1);
            _mm_stream_ps((float *) dp,
                          _mm_castsi128_ps(_mm_packs_epi32(val1,val2)));
            //dp[0] = _mm_packs_epi32(val1,val2);
          }
      }
  _mm_setcsr(mxcsr_orig); // Restore rounding control bits
}

/*****************************************************************************/
/* EXTERN                avx_xfer_irrev_decoded_block32                      */
/*****************************************************************************/

void
  avx_xfer_irrev_decoded_block32(kdu_int32 *src, float **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max, float delta)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  float fscale = delta;
  if (K_max <= 31)
    fscale /= (float)(1<<(31-K_max));
  else
    fscale *= (float)(1<<(K_max-31));
  int mxcsr_orig = _mm_getcsr();
  int mxcsr_cur = mxcsr_orig & ~(3<<13); // Reset rounding control bits
  _mm_setcsr(mxcsr_cur);
  __m256 vec_scale = _mm256_set1_ps(fscale);
  __m256 smask=_mm256_castsi256_ps(_mm256_set1_epi32(0x80000000));
  for (; height > 0; height--, dst_refs++, src+=width)
    { 
      float *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c += 8)
        { 
          __m256 ival = *((__m256 *)(src+c)); // Aligned 256-bit load
          __m256 sval = _mm256_and_ps(ival,smask); // Save sign bits
          __m256 mval = _mm256_andnot_ps(smask,ival); // Remove sign
          __m256 fval = _mm256_cvtepi32_ps(_mm256_castps_si256(mval));
          fval = _mm256_mul_ps(fval,vec_scale);
          _mm256_stream_ps((float *)(dst+c), _mm256_or_ps(fval,sval));
          //*((__m256 *)(dst+c)) = _mm256_or_ps(fval,sval);
        }
    }
  _mm_setcsr(mxcsr_orig); // Restore rounding control bits
}

#endif // !KDU_NO_AVX
