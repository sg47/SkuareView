/*****************************************************************************/
// File: x86_encoder_local.h [scope = CORESYS/CODING]
// Version: Kakadu, V7.2.1
// Author: David Taubman
// Last Revised: 28 March, 2013
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
data between the block coder and DWT line-based processing engine.  The
implementation here is based on MMX/SSE/SSE2/SSSE3/AVX intrinsics.  These
can be compiled under GCC or .NET and are compatible with both 32-bit and
64-bit builds.  AVX options are imported from the separately compiled file,
"avx_coder_local.cpp" so that the entire code-base need not be dependent
upon the availability of AVX support by the processor and OS.
   This file contains optimizations for the forward (quantization)
transfer of data from lines to code-blocks.
******************************************************************************/
#ifndef X86_ENCODER_LOCAL_H
#define X86_ENCODER_LOCAL_H

#ifndef KDU_NO_SSSE3
#  include <tmmintrin.h>
#elif (!defined KDU_NO_SSE)
#  include <emmintrin.h>
#endif


/* ========================================================================= */
/*                      SIMD functions used for encoding                     */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                   ..._quantize_rev_block16                         */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static kdu_int32
  ssse3_quantize_rev_block16(kdu_int32 *dst, kdu_int16 **src_refs,
                             int src_offset, int width, int height, int K_max)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  assert(K_max <= 15);
  __m128i upshift = _mm_cvtsi32_si128(15-K_max);
  __m128i smask = _mm_setzero_si128();
  __m128i or_val = _mm_setzero_si128();
  __m128i val1, val2, sign1, sign2;
  smask = _mm_slli_epi32(_mm_cmpeq_epi32(smask,smask),31); // -> 0x80000000
  for (; height > 0; height--, src_refs++, dst+=width)
    { 
      __m128i *sp = (__m128i *)(*src_refs + src_offset);
      __m128i *dp = (__m128i *)dst;
      for (int c=width; c > 0; c-=8, sp++, dp+=2)
        { 
          __m128i in = *sp;
          val1 = _mm_unpacklo_epi16(_mm_setzero_si128(),in);
          val2 = _mm_unpackhi_epi16(_mm_setzero_si128(),in);
          sign1=_mm_and_si128(smask,val1);   sign2=_mm_and_si128(smask,val2);
          val1=_mm_abs_epi32(val1);          val2=_mm_abs_epi32(val2);
          val1=_mm_sll_epi32(val1,upshift);  val2=_mm_sll_epi32(val2,upshift);
          or_val=_mm_or_si128(or_val,val1);  or_val=_mm_or_si128(or_val,val2);
          val1=_mm_or_si128(val1,sign1);     val2=_mm_or_si128(val2,sign2);
          dp[0] = val1;  dp[1] = val2;
        }
    }
  val1 = _mm_srli_si128(or_val,8);
  val1 = _mm_or_si128(val1,or_val); // Leave 2 OR'd dwords in low part of val1
  val2 = _mm_srli_si128(val1,4);
  val1 = _mm_or_si128(val1,val2); // Leave 1 OR'd dword in low part of val1
  return _mm_cvtsi128_si32(val1);
}
#  define SSSE3_SET_BLOCK_QUANT_REV16(_tgt,_kmax) \
          if ((kdu_mmx_level >= 4) && (_kmax <= 15)) \
            _tgt = ssse3_quantize_rev_block16;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_BLOCK_QUANT_REV16(_tgt,_kmax) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_QUANT_REV16(_tgt,_tr,_vf,_hf,_kmax) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSSE3_SET_BLOCK_QUANT_REV16(_tgt,_kmax); \
      } \
  }

/*****************************************************************************/
/* STATIC                  ..._quantize_rev_block32                          */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static kdu_int32
  ssse3_quantize_rev_block32(kdu_int32 *dst, kdu_int32 **src_refs,
                             int src_offset, int width, int height, int K_max)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i upshift = _mm_cvtsi32_si128(31-K_max);
  __m128i or_val = _mm_setzero_si128();
  __m128i smask = _mm_setzero_si128();
  __m128i val1, val2, sign1, sign2;
  smask = _mm_slli_epi32(_mm_cmpeq_epi32(smask,smask),31); // -> 0x80000000
  for (; height > 0; height--, src_refs++, dst+=width)
    { 
      __m128i *sp = (__m128i *)(*src_refs + src_offset);
      __m128i *dp = (__m128i *)dst;
      for (int c=width; c > 0; c-=8, sp+=2, dp+=2)
        { 
          val1 = sp[0];   val2 = sp[1];
          sign1=_mm_and_si128(smask,val1);   sign2=_mm_and_si128(smask,val2);
          val1=_mm_abs_epi32(val1);          val2=_mm_abs_epi32(val2);
          val1=_mm_sll_epi32(val1,upshift);  val2=_mm_sll_epi32(val2,upshift);
          or_val=_mm_or_si128(or_val,val1);  or_val=_mm_or_si128(or_val,val2);
          val1=_mm_or_si128(val1,sign1);     val2=_mm_or_si128(val2,sign2);
          dp[0] = val1;  dp[1] = val2;
        }
    }
  val1 = _mm_srli_si128(or_val,8);
  val1 = _mm_or_si128(val1,or_val); // Leave 2 OR'd dwords in low part of val1
  val2 = _mm_srli_si128(val1,4);
  val1 = _mm_or_si128(val1,val2); // Leave 1 OR'd dword in low part of val1
  return _mm_cvtsi128_si32(val1);
}
#  define SSSE3_SET_BLOCK_QUANT_REV32(_tgt) \
          if (kdu_mmx_level >= 4) _tgt=ssse3_quantize_rev_block32;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_BLOCK_QUANT_REV32(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_QUANT_REV32(_tgt,_tr,_vf,_hf) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSSE3_SET_BLOCK_QUANT_REV32(_tgt); \
      } \
  }

/*****************************************************************************/
/* STATIC                  ..._quantize_irrev_block16                        */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static kdu_int32
  sse2_quantize_irrev_block16(kdu_int32 *dst, kdu_int16 **src_refs,
                              int src_offset, int width, int height,
                              int K_max, float delta)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  float fscale = 1.0F / (delta * (float)(1<<KDU_FIX_POINT) * (float)(1<<16));
  if (K_max <= 31)
    fscale *= (float)(1<<(31-K_max));
  else
    fscale /= (float)(1<<(K_max-31));
  __m128 fval1, fval2, pscale = _mm_set1_ps(fscale);
  __m128i or_val = _mm_setzero_si128();
  __m128i smask = _mm_setzero_si128();
  __m128i val1, val2, sign1, sign2;
  smask = _mm_slli_epi32(_mm_cmpeq_epi32(smask,smask),31); // -> 0x80000000
  for (; height > 0; height--, src_refs++, dst+=width)
    { 
      __m128i *sp = (__m128i *)(*src_refs + src_offset);
      __m128i *dp = (__m128i *)dst;
      for (int c=width; c > 0; c-=8, sp++, dp+=2)
        { 
          __m128i in = *sp;
          val1 = _mm_unpacklo_epi16(_mm_setzero_si128(),in);
          val2 = _mm_unpackhi_epi16(_mm_setzero_si128(),in);
          sign1 = _mm_and_si128(smask,val1); sign2 = _mm_and_si128(smask,val2);
          fval1 = _mm_cvtepi32_ps(val1);     fval2 = _mm_cvtepi32_ps(val2);
          fval1 = _mm_mul_ps(fval1,pscale);  fval2 = _mm_mul_ps(fval2,pscale);
          fval1 = _mm_xor_ps(fval1,_mm_castsi128_ps(sign1));
          fval2 = _mm_xor_ps(fval2,_mm_castsi128_ps(sign2));
          val1 = _mm_cvttps_epi32(fval1);    val2 = _mm_cvttps_epi32(fval2);
          or_val=_mm_or_si128(or_val,val1);  or_val=_mm_or_si128(or_val,val2);
          val1 = _mm_or_si128(val1,sign1);   val2 = _mm_or_si128(val2,sign2);
          dp[0] = val1;  dp[1] = val2;
        }
    }
  val1 = _mm_srli_si128(or_val,8);
  val1 = _mm_or_si128(val1,or_val); // Leave 2 OR'd dwords in low part of val1
  val2 = _mm_srli_si128(val1,4);
  val1 = _mm_or_si128(val1,val2); // Leaves 1 OR'd dword in low part of val1
  return _mm_cvtsi128_si32(val1);
}
#  define SSE2_SET_BLOCK_QUANT_IRREV16(_tgt,_kmax) \
          if (kdu_mmx_level >= 4) _tgt=sse2_quantize_irrev_block16;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_QUANT_IRREV16(_tgt,_kmax) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_QUANT_IRREV16(_tgt,_tr,_vf,_hf,_kmax) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSE2_SET_BLOCK_QUANT_IRREV16(_tgt,_kmax); \
      } \
  }

/*****************************************************************************/
/* STATIC                  ..._quantize_irrev_block32                        */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static kdu_int32
  sse2_quantize_irrev_block32(kdu_int32 *dst, float **src_refs,
                              int src_offset, int width, int height,
                              int K_max, float delta)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  float fscale = 1.0F / delta;
  if (K_max <= 31)
    fscale *= (float)(1<<(31-K_max));
  else
    fscale /= (float)(1<<(K_max-31));
  __m128 fval1, fval2, fsign1, fsign2, pscale = _mm_set1_ps(fscale);
  __m128i or_val = _mm_setzero_si128();
  __m128i imask = _mm_setzero_si128();
  imask = _mm_slli_epi32(_mm_cmpeq_epi32(imask,imask),31); // -> 0x80000000
  __m128 fmask = _mm_castsi128_ps(imask);
  __m128i val1, val2;
  for (; height > 0; height--, src_refs++, dst+=width)
    { 
      __m128 *sp = (__m128 *)(*src_refs + src_offset);
      __m128i *dp = (__m128i *)dst;
      for (int c=width; c > 0; c-=8, sp+=2, dp+=2)
        { 
          fval1 = sp[0];  fval2 = sp[1];
          fsign1 = _mm_and_ps(fmask,fval1);  fsign2 = _mm_and_ps(fmask,fval2);
          fval1 = _mm_mul_ps(fval1,pscale);  fval2 = _mm_mul_ps(fval2,pscale);
          fval1 = _mm_xor_ps(fval1,fsign1);  fval2 = _mm_xor_ps(fval2,fsign2);
          val1 = _mm_cvttps_epi32(fval1);    val2 = _mm_cvttps_epi32(fval2);
          or_val=_mm_or_si128(or_val,val1);  or_val=_mm_or_si128(or_val,val2);
          val1 = _mm_or_si128(val1,_mm_castps_si128(fsign1));
          val2 = _mm_or_si128(val2,_mm_castps_si128(fsign2));
          dp[0] = val1;  dp[1] = val2;
        }
    }
  val1 = _mm_srli_si128(or_val,8);
  val1 = _mm_or_si128(val1,or_val); // Leave 2 OR'd dwords in low part of val1
  val2 = _mm_srli_si128(val1,4);
  val1 = _mm_or_si128(val1,val2); // Leaves 1 OR'd dword in low part of val1
  return _mm_cvtsi128_si32(val1);
}
#  define SSE2_SET_BLOCK_QUANT_IRREV32(_tgt) \
          if (kdu_mmx_level >= 4) _tgt=sse2_quantize_irrev_block32;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_QUANT_IRREV32(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_QUANT_IRREV32(_tgt,_tr,_vf,_hf) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSE2_SET_BLOCK_QUANT_IRREV32(_tgt); \
      } \
  }

#endif // X86_ENCODER_LOCAL_H
