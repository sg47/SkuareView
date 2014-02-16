/*****************************************************************************/
// File: x86_v_expand_local.h [scope = APPS/V_DECOMPRESSOR]
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
   SIMD accelerated data transfer functions for transferring decompressed
line buffers to a frame buffer within the "kdu_v_expand" demo application.
You can replicate this type of thing in your own application if required.
******************************************************************************/

#ifndef X86_V_EXPAND_LOCAL_H
#define X86_V_EXPAND_LOCAL_H

#ifndef KDU_NO_SSE
#  include <emmintrin.h>
#endif // !KDU_NO_SSE

/*****************************************************************************/
/* INLINE                    kd_simd_xfer_to_bytes                           */
/*****************************************************************************/

static inline bool
  kd_simd_xfer_to_bytes(kdu_line_buf &line, void *dst, int width,
                        int precision, bool is_signed)
{
#ifdef KDU_NO_SSE
  return false;
#else
  if (kdu_mmx_level < 2)
    return false;

  __m128i out, *dp = (__m128i *) dst;
  if (line.get_buf16() != NULL)
    { // Transfer 16-bit samples to 8-bit bytes
      __m128i *sp = (__m128i *)line.get_buf16();
      if (line.is_absolute())
        { 
          if (precision > 8)
            return false; // Don't bother with this less common case
          __m128i upshift = _mm_cvtsi32_si128(8-precision);
          __m128i pre_off = _mm_set1_epi16(128);
          __m128i post_off = _mm_set1_epi8((is_signed)?128:0);
          while (true)
            { 
              __m128i val1=sp[0], val2=sp[1];
              val1 = _mm_add_epi16(_mm_sra_epi16(val1,upshift),pre_off);
              val2 = _mm_add_epi16(_mm_sra_epi16(val2,upshift),pre_off);
              out = _mm_subs_epi8(_mm_packus_epi16(val1,val2),post_off);
              if (width < 16)
                break;
              _mm_storeu_si128(dp,out);
              sp += 2; dp++; width-=16;
            }
        }
      else
        { // Fixed-point 16-bit samples
          kdu_int32 offset = 1 << (KDU_FIX_POINT-1);
          offset += (1 << (KDU_FIX_POINT-8)) >> 1;
          __m128i pre_off = _mm_set1_epi16(offset);
          __m128i post_off = _mm_set1_epi8((is_signed)?128:0);
          while (true)
            { 
              __m128i val1=sp[0], val2=sp[1];
              val1=_mm_srai_epi16(_mm_add_epi16(val1,pre_off),KDU_FIX_POINT-8);
              val2=_mm_srai_epi16(_mm_add_epi16(val2,pre_off),KDU_FIX_POINT-8);
              out = _mm_subs_epi8(_mm_packus_epi16(val1,val2),post_off);
              if (width < 16)
                break;
              _mm_storeu_si128(dp,out);
              sp += 2; dp++; width-=16;
            }
        }
    }
  else
    { // 32-bit samples
      return false; // Don't bother implementing this right now
    }

  // Before returning, we need to store the final `width' samples from `out'
  kdu_byte *dp8 = (kdu_byte *)dp;
  for (; width > 0; width-=4, dp8+=4, out=_mm_srli_si128(out,4))
    { 
      kdu_int32 out32 = _mm_cvtsi128_si32(out);
      dp8[0] = (kdu_byte) out32;
      if (width > 1)
        {
          dp8[1] = (kdu_byte)(out32>>8);
          if (width > 2)
            {
              dp8[2] = (kdu_byte)(out32>>16);
              if (width > 3)
                dp8[3] = (kdu_byte)(out32>>24);
            }
        }
    }
  return true;
#endif // !KDU_NO_SSE
}

#endif // X86_V_EXPAND_LOCAL_H
