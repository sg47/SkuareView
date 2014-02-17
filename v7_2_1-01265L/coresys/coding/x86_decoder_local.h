/*****************************************************************************/
// File: x86_decoder_local.h [scope = CORESYS/CODING]
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
   This file contains optimizations for the reverse (dequantization)
transfer of data from code-blocks to lines.
******************************************************************************/
#ifndef X86_DECODER_LOCAL_H
#define X86_DECODER_LOCAL_H

#ifndef KDU_NO_SSSE3
#  include <tmmintrin.h>
#elif (!defined KDU_NO_SSE)
#  include <emmintrin.h>
#endif

/* ========================================================================= */
/*                      SIMD functions used for decoding                     */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                    ..._zero_decoded_block16                        */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_zero_decoded_block16(kdu_int16 **dst_refs, int dst_offset,
                            int width, int height)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i zero_val = _mm_setzero_si128();
  if (!(width & 31))
    { // 4-fold unwrapped loop
      for (; height > 0; height--, dst_refs++)
        { 
          kdu_int16 *dst = *dst_refs + dst_offset;
          for (int c=0; c < width; c += 32)
            { 
              *((__m128i *)(dst+c)) = zero_val;
              *((__m128i *)(dst+c+8)) = zero_val;
              *((__m128i *)(dst+c+16)) = zero_val;
              *((__m128i *)(dst+c+24)) = zero_val;
            }
        }
    }
  else
    { 
      for (; height > 0; height--, dst_refs++)
        { 
          kdu_int16 *dst = *dst_refs + dst_offset;
          for (int c=0; c < width; c += 8)
            *((__m128i *)(dst+c)) = zero_val;
        }
    }
}
#  define SSE2_SET_BLOCK_ZERO16(_tgt) \
          if (kdu_mmx_level >= 2) _tgt=sse2_zero_decoded_block16;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_ZERO16(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_ZERO16(_tgt) \
  { \
    SSE2_SET_BLOCK_ZERO16(_tgt); \
  }

/*****************************************************************************/
/* STATIC                   ..._zero_decoded_block32                         */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_zero_decoded_block32(kdu_int32 **dst_refs, int dst_offset,
                            int width, int height)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i zero_val = _mm_setzero_si128();
  for (; height > 0; height--, dst_refs++)
    { 
      kdu_int32 *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c += 8)
        { 
          *((__m128i *)(dst+c))   = zero_val;
          *((__m128i *)(dst+c+4)) = zero_val;
        }
    }
}
#  define SSE2_SET_BLOCK_ZERO32(_tgt) \
          if (kdu_mmx_level >= 2) _tgt=sse2_zero_decoded_block32;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_ZERO32(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_ZERO32(_tgt) \
  { \
    SSE2_SET_BLOCK_ZERO32(_tgt); \
  }

/*****************************************************************************/
/* STATIC                 ..._xfer_rev_decoded_block16                       */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static void
  ssse3_xfer_rev_decoded_block16(kdu_int32 *src, kdu_int16 **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i downshift = _mm_cvtsi32_si128(31-K_max);
  __m128i smask = _mm_setzero_si128();
  smask = _mm_slli_epi32(_mm_cmpeq_epi32(smask,smask),31); // -> 0x80000000
  smask = _mm_sra_epi32(smask,downshift); // Extends sign bit mask
  for (; height > 0; height--, dst_refs++, src+=width)
    { 
      kdu_int16 *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c+=8)
        { 
          __m128i val1 = _mm_sra_epi32(*((__m128i *)(src+c)),downshift);
          __m128i val2 = _mm_sra_epi32(*((__m128i *)(src+c+4)),downshift);
          __m128i signs1 = _mm_and_si128(val1,smask); // Save sign bit
          val1 = _mm_abs_epi32(val1);
          val1 = _mm_add_epi32(val1,signs1); // Leaves 2's complement words
          __m128i signs2 = _mm_and_si128(val2,smask); // Save sign bit
          val2 = _mm_abs_epi32(val2);
          val2 = _mm_add_epi32(val2,signs2); // Leaves 2's complement words
          *((__m128i *)(dst+c)) = _mm_packs_epi32(val1,val2);
        }
    }
}
#  define SSSE3_SET_BLOCK_XFER_REV16(_tgt,_kmax) \
          if (kdu_mmx_level >= 4) _tgt=ssse3_xfer_rev_decoded_block16;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_BLOCK_XFER_REV16(_tgt,_kmax) /* Do nothing */
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 4))
static void
  sse2_xfer_rev_decoded_block16(kdu_int32 *src, kdu_int16 **dst_refs,
                                int dst_offset, int width, int height,
                                int K_max)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i downshift = _mm_cvtsi32_si128(31-K_max);
  __m128i comp = _mm_setzero_si128(); // Avoid compiler warnings
  comp = _mm_cmpeq_epi32(comp,comp); // Fill with FF's
  __m128i ones = _mm_srli_epi32(comp,31); // Set each DWORD equal to 1
  __m128i kmax = _mm_cvtsi32_si128(K_max);
  comp = _mm_sll_epi32(comp,kmax); // Leaves 1+downshift 1's in MSB's
  comp = _mm_or_si128(comp,ones); // `comp' now holds the amount we have to
        // add after inverting the bits of a downshifted sign-mag quantity
        // which was negative, to restore the correct 2's complement value.
  for (; height > 0; height--, dst_refs++, src+=width)
    { 
      kdu_int16 *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c += 8)
        { 
          __m128i val1 = *((__m128i *)(src+c));
          __m128i val2 = *((__m128i *)(src+c+4));
          __m128i ref = _mm_setzero_si128();
          ref = _mm_cmpgt_epi32(ref,val1); // Fills DWORDS with 1's if val<0
          val1 = _mm_xor_si128(val1,ref);
          val1 = _mm_sra_epi32(val1,downshift);
          ref = _mm_and_si128(ref,comp); // Leave the bits we need to add
          val1 = _mm_add_epi32(val1,ref); // Finish conversion to 2's comp
          ref = _mm_setzero_si128();
          ref = _mm_cmpgt_epi32(ref,val2); // Fills DWORDS with 1's if val<0
          val2 = _mm_xor_si128(val2,ref);
          val2 = _mm_sra_epi32(val2,downshift);
          ref = _mm_and_si128(ref,comp); // Leave the bits we need to add
          val2 = _mm_add_epi32(val2,ref); // Finish conversion to 2's comp
              
          *((__m128i *)(dst+c)) = _mm_packs_epi32(val1,val2);
        }
    }
}
#  define SSE2_SET_BLOCK_XFER_REV16(_tgt,_kmax) \
          if (kdu_mmx_level >= 2) _tgt=sse2_xfer_rev_decoded_block16;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_XFER_REV16(_tgt,_kmax) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_XFER_REV16(_tgt,_tr,_vf,_hf,_kmax) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSE2_SET_BLOCK_XFER_REV16(_tgt,_kmax); \
        SSSE3_SET_BLOCK_XFER_REV16(_tgt,_kmax); \
      } \
  }

/*****************************************************************************/
/* STATIC                ..._xfer_rev_decoded_block32                        */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static void
  ssse3_xfer_rev_decoded_block32(kdu_int32 *src, kdu_int32 **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max)
{
  assert(!(width & 7)); // Width must be a multiple of 8
  __m128i downshift = _mm_cvtsi32_si128(31-K_max);
  __m128i smask = _mm_setzero_si128();
  smask = _mm_slli_epi32(_mm_cmpeq_epi32(smask,smask),31); // -> 0x80000000
  smask = _mm_sra_epi32(smask,downshift); // Extends sign bit mask
  for (; height > 0; height--, dst_refs++, src+=width)
    { 
      kdu_int32 *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c += 8)
        { 
          __m128i val1 = _mm_sra_epi32(*((__m128i *)(src+c)),downshift);
          __m128i val2 = _mm_sra_epi32(*((__m128i *)(src+c+4)),downshift);
          __m128i signs1 = _mm_and_si128(val1,smask); // Save sign info
          val1 = _mm_abs_epi32(val1); // -ve values map to
          *((__m128i *)(dst+c)) = _mm_add_epi32(val1,signs1);
          __m128i signs2 = _mm_and_si128(val2,smask); // Save sign info
          val2 = _mm_abs_epi32(val2); // -ve values map to
          *((__m128i *)(dst+c+4)) = _mm_add_epi32(val2,signs2);
      }
    }
}
#  define SSSE3_SET_BLOCK_XFER_REV32(_tgt) \
          if (kdu_mmx_level >= 4) _tgt=ssse3_xfer_rev_decoded_block32;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_BLOCK_XFER_REV32(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_XFER_REV32(_tgt,_tr,_vf,_hf) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSSE3_SET_BLOCK_XFER_REV32(_tgt); \
      } \
  }

/*****************************************************************************/
/* STATIC               ..._xfer_irrev_decoded_block16                       */
/*****************************************************************************/

#ifndef KDU_NO_AVX
extern void
  avx_xfer_irrev_decoded_block16(kdu_int32 *src, kdu_int16 **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max, float delta);
#  define AVX_SET_BLOCK_XFER_IRREV16(_tgt,_kmax) \
          if (kdu_mmx_level >= 6) _tgt=avx_xfer_irrev_decoded_block16;
#else // !KDU_NO_AVX
#  define AVX_SET_BLOCK_XFER_IRREV16(_tgt,_kmax) /* Do nothing */
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 6))
static void
  sse2_xfer_irrev_decoded_block16(kdu_int32 *src, kdu_int16 **dst_refs,
                                  int dst_offset, int width, int height,
                                  int K_max, float delta)
{ 
  float fscale = delta * (float)(1<<KDU_FIX_POINT);
  if (K_max <= 31)
    fscale /= (float)(1<<(31-K_max));
  else
    fscale *= (float)(1<<(K_max-31));
  int mxcsr_orig = _mm_getcsr();
  int mxcsr_cur = mxcsr_orig & ~(3<<13); // Reset rounding control bits
  _mm_setcsr(mxcsr_cur);
  __m128 vec_scale = _mm_load1_ps(&fscale);
  __m128i comp = _mm_setzero_si128(); // Avoid compiler warnings
  __m128i ones = _mm_cmpeq_epi32(comp,comp); // Fill with FF's
  comp = _mm_slli_epi32(ones,31); // Each DWORD  now holds 0x80000000
  ones = _mm_srli_epi32(ones,31); // Each DWORD now holds 1
  comp = _mm_or_si128(comp,ones); // Each DWORD now holds 0x800000001
  for (; height > 0; height--, dst_refs++, src+=width)
    { 
      kdu_int16 *dst = *dst_refs + dst_offset;
      for (int c=0; c < width; c += 8)
        { 
          __m128i val1 = *((__m128i *)(src+c));
          __m128i ref = _mm_setzero_si128();
          ref = _mm_cmpgt_epi32(ref,val1); // Fills DWORDS with 1's if val<0
          val1 = _mm_xor_si128(val1,ref);
          ref = _mm_and_si128(ref,comp); // Leave the bits we need to add
          val1 = _mm_add_epi32(val1,ref); // Finish conversion to 2's comp
          __m128 fval1 = _mm_cvtepi32_ps(val1);
          fval1 = _mm_mul_ps(fval1,vec_scale);
          val1 = _mm_cvtps_epi32(fval1);

          __m128i val2 = *((__m128i *)(src+c+4));
          ref = _mm_setzero_si128();
          ref = _mm_cmpgt_epi32(ref,val2); // Fills DWORDS with 1's if val<0
          val2 = _mm_xor_si128(val2,ref);
          ref = _mm_and_si128(ref,comp); // Leave the bits we need to add
          val2 = _mm_add_epi32(val2,ref); // Finish conversion to 2's comp
          __m128 fval2 = _mm_cvtepi32_ps(val2);
          fval2 = _mm_mul_ps(fval2,vec_scale);
          val2 = _mm_cvtps_epi32(fval2);

          *((__m128i *)(dst+c)) = _mm_packs_epi32(val1,val2);
        }
    }
  _mm_setcsr(mxcsr_orig); // Restore rounding control bits
}
#  define SSE2_SET_BLOCK_XFER_IRREV16(_tgt,_kmax) \
          if (kdu_mmx_level >= 2) _tgt=sse2_xfer_irrev_decoded_block16;
#else // !KDU_NO_SSE
#  define SSE2_SET_BLOCK_XFER_IRREV16(_tgt,_kmax) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_XFER_IRREV16(_tgt,_tr,_vf,_hf,_kmax) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        SSE2_SET_BLOCK_XFER_IRREV16(_tgt,_kmax); \
        AVX_SET_BLOCK_XFER_IRREV16(_tgt,_kmax); \
      } \
  }

/*****************************************************************************/
/* STATIC                ..._xfer_irrev_decoded_block32                      */
/*****************************************************************************/

#ifndef KDU_NO_AVX
extern void
  avx_xfer_irrev_decoded_block32(kdu_int32 *src, float **dst_refs,
                                 int dst_offset, int width, int height,
                                 int K_max, float delta);
#  define AVX_SET_BLOCK_XFER_IRREV32(_tgt) \
          if (kdu_mmx_level >= 6) _tgt=avx_xfer_irrev_decoded_block32;
#else // !KDU_NO_AVX
#  define AVX_SET_BLOCK_XFER_IRREV32(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_BLOCK_XFER_IRREV32(_tgt,_tr,_vf,_hf) \
  { \
    if (!(_tr || _vf || _hf)) \
      { \
        AVX_SET_BLOCK_XFER_IRREV32(_tgt); \
      } \
  }

#endif // X86_DECODER_LOCAL_H
