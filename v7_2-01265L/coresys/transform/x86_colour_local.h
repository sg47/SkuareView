/*****************************************************************************/
// File: x86_colour_local.h [scope = CORESYS/TRANSFORMS]
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
   Implements the forward and reverse colour transformations -- both the
reversible (RCT) and the irreversible (ICT = RGB to YCbCr) -- using
MMX/SSE/SSE2 intrinsics.  These can be compiled under GCC or .NET and are
compatible with both 32-bit and 64-bit builds.  Note, however, that the
SSE/SSE2 intrinsics require 16-byte stack alignment, which might not be
guaranteed by GCC builds if the code is called from outside (e.g., from a
Java Virtual Machine, which has poorer stack alignment).  This is an ongoing
problem with GCC, but it does not affect stand-alone applications.  As a
result, you are recommended to use "gcc_colour_mmx_local.h" for both
32-bit and 64-bit GCC builds, since it imposes no stack alignment
requirements.  For MSVC builds you will need the implementation
in this file for 64-bit builds, although both this file and
"msvc_colour_mmx_local.h" will work for 32-bit builds.
******************************************************************************/

#ifndef X86_COLOUR_LOCAL_H
#define X86_COLOUR_LOCAL_H

#if (!defined KDU_NO_SSSE3)
#  include <tmmintrin.h>
#else
#  include <emmintrin.h>
#endif


// The constants below are used for SIMD processing with pre-offsets.
// Note that: ALPHA_R=0.299, ALPHA_B=0.114, ALPHA_G=1-ALPHA_R-ALPHA_B,
//            ALPHA_RB=ALPHA_R+ALPHA_B, CB_FACT=0.564, CR_FACT=0.713,
//     CR_FACT_R=1.402, CB_FACT_B=1.772, CR_FACT_G=0.714, CB_FACT_G=0.344
// Forward transform is:
//     Y = ALPHA_R * R + ALPHA_G * G + ALPHA_B * B
//    Cb = CB_FACT * (B-Y)
//    Cr - CR_FACT * (R-Y0
// Inverse transform is:
//     R = Y + CR_FACT_R * Cr
//     B = Y + CB_FACT_B * Cb
//     G = Y - CR_FACT_G * Cr - CB_FACT_G * Cb
// The vector values are related to the non-vector values through:
//     vec_CBfact  = 1-CB_FACT   = 0.436, vec_CRfact  = 1-CR_FACT   = -0.287
//     vec_CRfactR = CR_FACT_R-1 = 0.402, vec_CBfactB = CB_FACT_B-2 = -0.218
//     vec_CRfactG = 1-CR_FACT_G = 0.286, vec_CBfactG = -CB_FACT_G  = -0.344
// The MMX and SSE2 SIMD optimization branches then implement the following:
//    Cb = (1-vec_CBfact)*(B-Y)
//    Cr = (1-vec_CRfact)*(R-Y)
//     R = Y + (vec_CRfactR+1)*Cr
//     B = Y + (vec_CBfactB+2)*Cb
//     G = Y + (vec_CRfactG-1)*Cr + vec_CbfactG*Cb

#define vec64_alphaR   ((kdu_int16)(0.5+ALPHA_R*(1<<16)))
#define vec64_alphaB   ((kdu_int16)(0.5+ALPHA_B*(1<<16)))
#define vec64_alphaRB  ((kdu_int16)(0.5+(ALPHA_RB)*(1<<16)))
#define vec64_CBfact   ((kdu_int16)(0.5+(1-CB_FACT)*(1<<16)))
#define vec64_CRfact   ((kdu_int16)(0.5+(1-CR_FACT)*(1<<16)))
#define vec64_CRfactR  ((kdu_int16)(0.5+(CR_FACT_R-1)*(1<<16)))
#define vec64_CBfactB  ((kdu_int16)(0.5+(CB_FACT_B-2)*(1<<16)))
#define vec64_CRfactG  ((kdu_int16)(0.5+(1-CR_FACT_G)*(1<<16)))
#define vec64_CBfactG  ((kdu_int16)(0.5-CB_FACT_G*(1<<16)))

#define vec128_alphaR  ((kdu_int16)(0.5+ALPHA_R*(1<<16)))
#define vec128_alphaB  ((kdu_int16)(0.5+ALPHA_B*(1<<16)))
#define vec128_alphaRB ((kdu_int16)(0.5+(ALPHA_RB)*(1<<16)))
#define vec128_CBfact  ((kdu_int16)(0.5+(1-CB_FACT)*(1<<16)))
#define vec128_CRfact  ((kdu_int16)(0.5+(1-CR_FACT)*(1<<16)))
#define vec128_CRfactR ((kdu_int16)(0.5+(CR_FACT_R-1)*(1<<16)))
#define vec128_CBfactB ((kdu_int16)(0.5+(CB_FACT_B-2)*(1<<16)))
#define vec128_CRfactG ((kdu_int16)(0.5+(1-CR_FACT_G)*(1<<16)))
#define vec128_CBfactG ((kdu_int16)(0.5-CB_FACT_G*(1<<16)))

// The constants below are used for SIMD processing with SSSE3 rounding.
// These vector values are related to the non-vector values through:
//  ssse3_CBfact  = CB_FACT     =  0.564; ssse3_CRfact  = CR_FACT     = 0.713
//  ssse3_CRfactR = CR_FACT_R-1 =  0.402; ssse3_CBfactB = CB_FACT_B-1 = 0.772
//  ssse3_CRfactG = -CR_FACT_G  = -0.714; ssse3_CBfactG = -CB_FACT_G  = -0.344
#define ssse3_alphaR      ((kdu_int16)(0.5+ALPHA_R*(1<<15)))
#define ssse3_alphaB      ((kdu_int16)(0.5+ALPHA_B*(1<<15)))
#define ssse3_alphaG      ((kdu_int16)(0.5+ALPHA_G*(1<<15)))
#define ssse3_CBfact      ((kdu_int16)(0.5+CB_FACT*(1<<15)))
#define ssse3_CRfact      ((kdu_int16)(0.5+CR_FACT*(1<<15)))
#define ssse3_CRfactR     ((kdu_int16)(0.5+(CR_FACT_R-1)*(1<<15)))
#define ssse3_CBfactB     ((kdu_int16)(0.5+(CB_FACT_B-1)*(1<<15)))
#define ssse3_neg_CRfactG ((kdu_int16)(0.5-CR_FACT_G*(1<<15)))
#define ssse3_neg_CBfactG ((kdu_int16)(0.5-CB_FACT_G*(1<<15)))

// The constants below are used for SIMD floating-point processing.  They
// are identical to those for SSSE3 integer processing with rounding, except
// that no offset is required for the CRfactR and CBfactB values.
#define vecps_alphaR      ((float) ALPHA_R)
#define vecps_alphaB      ((float) ALPHA_B)
#define vecps_alphaG      ((float) ALPHA_G)
#define vecps_CBfact      ((float) CB_FACT)
#define vecps_CRfact      ((float) CR_FACT)
#define vecps_CBfactB     ((float) CB_FACT_B)
#define vecps_CRfactR     ((float) CR_FACT_R)
#define vecps_neg_CBfactG ((float) -CB_FACT_G)
#define vecps_neg_CRfactG ((float) -CR_FACT_G)


/* ========================================================================= */
/*                        Now for the MMX functions                          */
/* ========================================================================= */

/*****************************************************************************/
/* STATIC                  ..._rgb_to_ycc_irrev16                            */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static void
  ssse3_rgb_to_ycc_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                           int samples)
{
  __m128i alpha_r = _mm_set1_epi16(ssse3_alphaR);
  __m128i alpha_b = _mm_set1_epi16(ssse3_alphaB);
  __m128i alpha_g = _mm_set1_epi16(ssse3_alphaG);
  __m128i cb_fact = _mm_set1_epi16(ssse3_CBfact);
  __m128i cr_fact = _mm_set1_epi16(ssse3_CRfact);
  for (int c=0; c < samples; c += 8)
    { 
      __m128i green = *((__m128i *)(src2 + c));
      __m128i y = _mm_mulhrs_epi16(green,alpha_g);
      __m128i red = *((__m128i *)(src1 + c));
      __m128i blue = *((__m128i *)(src3 + c));
      y = _mm_add_epi16(y,_mm_mulhrs_epi16(red,alpha_r));
      y = _mm_add_epi16(y,_mm_mulhrs_epi16(blue,alpha_b));
      *((__m128i *)(src1 + c)) = y;
      blue = _mm_sub_epi16(blue,y);
      *((__m128i *)(src2 + c)) = _mm_mulhrs_epi16(blue,cb_fact);
      red = _mm_sub_epi16(red,y);
      *((__m128i *)(src3 + c)) = _mm_mulhrs_epi16(red,cr_fact);
    }
}
#  define SSSE3_SET_RGB_TO_YCC_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 4) _tgt=ssse3_rgb_to_ycc_irrev16;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_RGB_TO_YCC_IRREV16(_tgt) /* Do nothing */
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 4))
static void
  sse2_rgb_to_ycc_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                          int samples)
{
  __m128i ones = _mm_set1_epi16(1);
  __m128i alpha_r = _mm_set1_epi16(vec128_alphaR);
  __m128i alpha_b = _mm_set1_epi16(vec128_alphaB);
  __m128i alpha_r_plus_b = _mm_set1_epi16(vec128_alphaRB);
  __m128i cb_fact = _mm_set1_epi16(vec128_CBfact);
  __m128i cr_fact = _mm_set1_epi16(vec128_CRfact);
  for (int c=0; c < samples; c += 8)
    {
      __m128i red = *((__m128i *)(src1 + c));
      __m128i blue = *((__m128i *)(src3 + c));
      __m128i tmp = _mm_adds_epi16(ones,ones); // Make a pre-offset of 2
      __m128i y = _mm_adds_epi16(red,tmp);  // Form pre-offset red channel
      y = _mm_mulhi_epi16(y,alpha_r); // Red contribution to Y
      tmp = _mm_add_epi16(tmp,tmp); // Form pre-offset of 4 in `tmp'
      tmp = _mm_adds_epi16(tmp,blue);
      tmp = _mm_mulhi_epi16(tmp,alpha_b); // Blue contribution to Y
      y = _mm_adds_epi16(y,tmp); // Add red and blue contributions to Y.
      __m128i green = *((__m128i *)(src2 + c));
      tmp = _mm_adds_epi16(green,ones); // Add pre-offset of 1 to the green channel
      tmp = _mm_mulhi_epi16(tmp,alpha_r_plus_b); // Green * (alphaR+alphaB)
      green = _mm_subs_epi16(green,tmp); // Forms green * (1-alphaR-alphaB)
      y = _mm_adds_epi16(y,green); // Forms final luminance channel
      *((__m128i *)(src1 + c)) = y;
      blue = _mm_subs_epi16(blue,y); // Forms Blue-Y in `blue'
      tmp = _mm_adds_epi16(blue,ones); // Add a pre-offset of 1 to Blue-Y
      tmp = _mm_mulhi_epi16(tmp,cb_fact);
      *((__m128i *)(src2 + c)) = _mm_subs_epi16(blue,tmp); // Forms CB = (blue-Y)*(1-CBfact)
      red = _mm_subs_epi16(red,y); // Forms Red-Y in `red'
      tmp = _mm_adds_epi16(red,ones); // Add a pre-offset of 1 to Red-Y
      tmp = _mm_adds_epi16(tmp,ones); // Make the pre-offset equal to 2
      tmp = _mm_mulhi_epi16(tmp,cr_fact); // Multiply (Red-Y) * CRfact
      *((__m128i *)(src3 + c)) = _mm_subs_epi16(red,tmp); // Forms CR = (red-Y)*(1-CRfact)
    }
}
#  define SSE2_SET_RGB_TO_YCC_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_rgb_to_ycc_irrev16;
#else // !KDU_NO_SSE
#  define SSE2_SET_RGB_TO_YCC_IRREV16(_tgt) /* Do nothing */
#endif

#if ((!defined KDU_NO_MMX64) && (KDU_MIN_MMX_LEVEL < 2))
static void
  mmx_rgb_to_ycc_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                         int samples)
{
  __m64 alpha_r = _mm_set1_pi16(vec64_alphaR);
  __m64 alpha_b = _mm_set1_pi16(vec64_alphaB);
  __m64 alpha_r_plus_b = _mm_set1_pi16(vec64_alphaRB);
  __m64 cb_fact = _mm_set1_pi16(vec64_CBfact);
  __m64 cr_fact = _mm_set1_pi16(vec64_CRfact);
  __m64 *sp1 = (__m64 *) src1;
  __m64 *sp2 = (__m64 *) src2;
  __m64 *sp3 = (__m64 *) src3;
  __m64 ones = _mm_set1_pi16(1);
  samples = (samples+3)>>2;
  for (int c=0; c < samples; c++)
    {
      __m64 red = sp1[c];
      __m64 blue = sp3[c];
      __m64 tmp = _mm_adds_pi16(ones,ones); // Make a pre-offset of 2
      __m64 y = _mm_adds_pi16(red,tmp);  // Form pre-offset red channel
      y = _mm_mulhi_pi16(y,alpha_r); // Red contribution to Y
      tmp = _mm_add_pi16(tmp,tmp); // Form pre-offset of 4 in `tmp'
      tmp = _mm_adds_pi16(tmp,blue);
      tmp = _mm_mulhi_pi16(tmp,alpha_b); // Blue contribution to Y
      y = _mm_adds_pi16(y,tmp); // Add red and blue contributions to Y.
      __m64 green = sp2[c];
      tmp = _mm_adds_pi16(green,ones); // Add pre-offset of 1 to the green channel
      tmp = _mm_mulhi_pi16(tmp,alpha_r_plus_b); // Green * (alphaR+alphaB)
      green = _mm_subs_pi16(green,tmp); // Forms green * (1-alphaR-alphaB)
      y = _mm_adds_pi16(y,green); // Forms final luminance channel
      sp1[c] = y;
      blue = _mm_subs_pi16(blue,y); // Forms Blue-Y in `blue'
      tmp = _mm_adds_pi16(blue,ones); // Add a pre-offset of 1 to Blue-Y
      tmp = _mm_mulhi_pi16(tmp,cb_fact);
      sp2[c] = _mm_subs_pi16(blue,tmp); // Forms CB = (blue-Y)*(1-CBfact)
      red = _mm_subs_pi16(red,y); // Forms Red-Y in `red'
      tmp = _mm_adds_pi16(red,ones); // Add a pre-offset of 1 to Red-Y
      tmp = _mm_adds_pi16(tmp,ones); // Make the pre-offset equal to 2
      tmp = _mm_mulhi_pi16(tmp,cr_fact); // Multiply (Red-Y) * CRfact
      sp3[c] = _mm_subs_pi16(red,tmp); // Forms CR = (red-Y)*(1-CRfact)
    }
  _mm_empty();
}
#  define MMX_SET_RGB_TO_YCC_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 1) _tgt=mmx_rgb_to_ycc_irrev16;
#else // !KDU_NO_MMX64
#  define MMX_SET_RGB_TO_YCC_IRREV16(_tgt) /* Do nothing */
#endif

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_IRREV16(_tgt) \
  { \
    MMX_SET_RGB_TO_YCC_IRREV16(_tgt); \
    SSE2_SET_RGB_TO_YCC_IRREV16(_tgt); \
    SSSE3_SET_RGB_TO_YCC_IRREV16(_tgt); \
  }

/*****************************************************************************/
/* STATIC                  ..._rgb_to_ycc_irrev32                            */
/*****************************************************************************/

#ifndef KDU_NO_AVX
extern void avx_rgb_to_ycc_irrev32(float *, float *, float *, int);
#  define AVX_SET_RGB_TO_YCC_IRREV32(_tgt) \
          if (kdu_get_mmx_level() >= 6) _tgt=avx_rgb_to_ycc_irrev32;
#else // !KDU_NO_AVX
#  define AVX_SET_RGB_TO_YCC_IRREV32(_tgt)
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 6))
static void
  sse2_rgb_to_ycc_irrev32(float *src1, float *src2, float *src3, int samples)
{
  __m128 alpha_r = _mm_set1_ps(vecps_alphaR);
  __m128 alpha_b = _mm_set1_ps(vecps_alphaB);
  __m128 alpha_g = _mm_set1_ps(vecps_alphaG);
  __m128 cb_fact = _mm_set1_ps(vecps_CBfact);
  __m128 cr_fact = _mm_set1_ps(vecps_CRfact);
  for (int c=0; c < samples; c += 4)
    { 
      __m128 green = *((__m128 *)(src2 + c));
      __m128 y = _mm_mul_ps(green,alpha_g);
      __m128 red = *((__m128 *)(src1 + c));
      __m128 blue = *((__m128 *)(src3 + c));
      y = _mm_add_ps(y,_mm_mul_ps(red,alpha_r));
      y = _mm_add_ps(y,_mm_mul_ps(blue,alpha_b));
      *((__m128 *)(src1 + c)) = y;
      blue = _mm_sub_ps(blue,y);
      *((__m128 *)(src2 + c)) = _mm_mul_ps(blue,cb_fact);
      red = _mm_sub_ps(red,y);
      *((__m128 *)(src3 + c)) = _mm_mul_ps(red,cr_fact);
    }
}
#  define SSE2_SET_RGB_TO_YCC_IRREV32(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_rgb_to_ycc_irrev32;
#else // !KDU_NO_SSE
#  define SSE2_SET_RGB_TO_YCC_IRREV32(_tgt)
#endif

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_IRREV32(_tgt) \
  { \
    SSE2_SET_RGB_TO_YCC_IRREV32(_tgt); \
    AVX_SET_RGB_TO_YCC_IRREV32(_tgt); \
  }

/*****************************************************************************/
/* STATIC                  ..._ycc_to_rgb_irrev16                            */
/*****************************************************************************/

#ifndef KDU_NO_SSSE3
static void
  ssse3_ycc_to_rgb_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                           int samples)
{
  __m128i cr_fact_r = _mm_set1_epi16(ssse3_CRfactR);
  __m128i cr_neg_fact_g = _mm_set1_epi16(ssse3_neg_CRfactG);
  __m128i cb_fact_b = _mm_set1_epi16(ssse3_CBfactB);
  __m128i cb_neg_fact_g = _mm_set1_epi16(ssse3_neg_CBfactG);
  for (int c=0; c < samples; c+=8)
    { 
      __m128i y = *((__m128i *)(src1 + c));
      __m128i tmp = *((__m128i *)(src3 + c)); // Load CR
      __m128i cr = tmp;
      tmp = _mm_mulhrs_epi16(tmp,cr_fact_r);
      tmp = _mm_add_epi16(tmp,cr);
      *((__m128i *)(src1 + c)) = _mm_adds_epi16(tmp,y); // Save Red
      cr = _mm_mulhrs_epi16(cr,cr_neg_fact_g);
      tmp = *((__m128i *)(src2 + c)); // Load CB
      __m128i cb = tmp;
      tmp = _mm_mulhrs_epi16(tmp,cb_fact_b);
      tmp = _mm_add_epi16(tmp,cb);
      *((__m128i *)(src3 + c)) = _mm_adds_epi16(tmp,y); // Save Blue
      cb = _mm_mulhrs_epi16(cb,cb_neg_fact_g);
      y = _mm_adds_epi16(y,cr);
      *((__m128i *)(src2 + c)) = _mm_adds_epi16(y,cb); // Save Green
    }
}
#  define SSSE3_SET_YCC_TO_RGB_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 4) _tgt=ssse3_ycc_to_rgb_irrev16;
#else // !KDU_NO_SSSE3
#  define SSSE3_SET_YCC_TO_RGB_IRREV16(_tgt)
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 4))
static void
  sse2_ycc_to_rgb_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                          int samples)
{
  __m128i ones = _mm_set1_epi16(1); // Set 1's in each 16-bit word
  __m128i twos = _mm_add_epi16(ones,ones); // Set 2's in each 16-bit word
  __m128i cr_fact_r = _mm_set1_epi16(vec128_CRfactR);
  __m128i cr_fact_g = _mm_set1_epi16(vec128_CRfactG);
  __m128i cb_fact_b = _mm_set1_epi16(vec128_CBfactB);
  __m128i cb_fact_g = _mm_set1_epi16(vec128_CBfactG);
  for (int c=0; c < samples; c += 8)
    {
      __m128i y = *((__m128i *)(src1 + c));
      __m128i cr = *((__m128i *)(src3 + c));
      __m128i red = _mm_adds_epi16(cr,ones); // Add pre-offset to CR
      red = _mm_mulhi_epi16(red,cr_fact_r); // Multiply by 0.402*2^16 (CRfactR) & divide by 2^16
      red = _mm_adds_epi16(red,cr); // Add CR again to make factor equivalent to 1.402
      *((__m128i *)(src1 + c)) = _mm_adds_epi16(red,y); // Add in luminance to get red & save
      __m128i green = _mm_adds_epi16(cr,twos); // Pre-offset of 2
      green = _mm_mulhi_epi16(green,cr_fact_g); // Multiply by 0.285864*2^16 (CRfactG) & divide by 2^16
      green = _mm_subs_epi16(green,cr); // Leaves the correct multiple of CR
      green = _mm_adds_epi16(green,y); // Y + scaled CR forms most of green
      __m128i cb = *((__m128i *)(src2 + c));
      __m128i blue = _mm_subs_epi16(cb,twos); // Pre-offset of -2
      blue = _mm_mulhi_epi16(blue,cb_fact_b); // Multiply by -0.228*2^16 (CBfactB) & divide by 2^16
      blue = _mm_adds_epi16(blue,cb); // Gets 0.772*Cb in blue
      blue = _mm_adds_epi16(blue,cb); // Gets 1.772*Cb in blue
      *((__m128i *)(src3 + c)) = _mm_adds_epi16(blue,y); // Add in luminance to get blue & save
      cb = _mm_subs_epi16(cb,twos); // Pre-offset of -2
      cb = _mm_mulhi_epi16(cb,cb_fact_g); // Multiply by -0.344136*2^16 (CBfactG) and divide by 2^16
      *((__m128i *)(src2 + c)) = _mm_adds_epi16(green,cb); // Complete and save the green channel
    }
}
#  define SSE2_SET_YCC_TO_RGB_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_ycc_to_rgb_irrev16;
#else // !KDU_NO_SSE
#  define SSE2_SET_YCC_TO_RGB_IRREV16(_tgt)
#endif

#if ((!defined KDU_NO_MMX64) && (KDU_MIN_MMX_LEVEL < 2))
static void
  mmx_ycc_to_rgb_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3, 
                         int samples)
{
  __m64 cr_fact_r = _mm_set1_pi16(vec64_CRfactR);
  __m64 cr_fact_g = _mm_set1_pi16(vec64_CRfactG);
  __m64 cb_fact_b = _mm_set1_pi16(vec64_CBfactB);
  __m64 cb_fact_g = _mm_set1_pi16(vec64_CBfactG);
  __m64 ones = _mm_set1_pi16(1); // Set 1's in each 16-bit word
  __m64 twos = _mm_add_pi16(ones,ones); // Set 2's in each 16-bit word
  __m64 *sp1 = (__m64 *) src1;
  __m64 *sp2 = (__m64 *) src2;
  __m64 *sp3 = (__m64 *) src3;
  samples = (samples+3)>>2;
  for (int c=0; c < samples; c++)
    {
      __m64 y = sp1[c];
      __m64 cr = sp3[c];
      __m64 red = _mm_adds_pi16(cr,ones); // Add pre-offset to CR
      red = _mm_mulhi_pi16(red,cr_fact_r); // Multiply by 0.402*2^16 (CRfactR) & divide by 2^16
      red = _mm_adds_pi16(red,cr); // Add CR again to make factor equivalent to 1.402
      sp1[c] = _mm_adds_pi16(red,y); // Add in luminance to get red & save
      __m64 green = _mm_adds_pi16(cr,twos); // Pre-offset of 2
      green = _mm_mulhi_pi16(green,cr_fact_g); // Multiply by 0.285864*2^16 (CRfactG) & divide by 2^16
      green = _mm_subs_pi16(green,cr); // Leaves the correct multiple of CR
      green = _mm_adds_pi16(green,y); // Y + scaled CR forms most of green
      __m64 cb = sp2[c];
      __m64 blue = _mm_subs_pi16(cb,twos); // Pre-offset of -2
      blue = _mm_mulhi_pi16(blue,cb_fact_b); // Multiply by -0.228*2^16 (CBfactB) & divide by 2^16
      blue = _mm_adds_pi16(blue,cb); // Gets 0.772*Cb in blue
      blue = _mm_adds_pi16(blue,cb); // Gets 1.772*Cb in blue
      sp3[c] = _mm_adds_pi16(blue,y); // Add in luminance to get blue & save
      cb = _mm_subs_pi16(cb,twos); // Pre-offset of -2
      cb = _mm_mulhi_pi16(cb,cb_fact_g); // Multiply by -0.344136*2^16 (CBfactG) and divide by 2^16
      sp2[c] = _mm_adds_pi16(green,cb); // Complete and save the green channel
    }
  _mm_empty(); // Clear MMX registers for use by FPU
}
#  define MMX_SET_YCC_TO_RGB_IRREV16(_tgt) \
          if (kdu_get_mmx_level() >= 1) _tgt=mmx_ycc_to_rgb_irrev16;
#else // !KDU_NO_MMX64
#  define MMX_SET_YCC_TO_RGB_IRREV16(_tgt)
#endif

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_IRREV16(_tgt) \
  { \
    MMX_SET_YCC_TO_RGB_IRREV16(_tgt); \
    SSE2_SET_YCC_TO_RGB_IRREV16(_tgt); \
    SSSE3_SET_YCC_TO_RGB_IRREV16(_tgt); \
  }

/*****************************************************************************/
/* STATIC                  ..._ycc_to_rgb_irrev32                            */
/*****************************************************************************/

#ifndef KDU_NO_AVX
extern void avx_ycc_to_rgb_irrev32(float *, float *, float *, int);
#  define AVX_SET_YCC_TO_RGB_IRREV32(_tgt) \
          if (kdu_get_mmx_level() >= 6) _tgt=avx_ycc_to_rgb_irrev32;
#else // !KDU_NO_AVX
#  define AVX_SET_YCC_TO_RGB_IRREV32(_tgt)
#endif

#if ((!defined KDU_NO_SSE) && (KDU_MIN_MMX_LEVEL < 6))  
static void
  sse2_ycc_to_rgb_irrev32(float *src1, float *src2, float *src3, int samples)
{
  __m128 cr_fact_r = _mm_set1_ps(vecps_CRfactR);
  __m128 neg_cr_fact_g = _mm_set1_ps(vecps_neg_CRfactG);
  __m128 cb_fact_b = _mm_set1_ps(vecps_CBfactB);
  __m128 neg_cb_fact_g = _mm_set1_ps(vecps_neg_CBfactG);
  for (int c=0; c < samples; c += 4)
    { 
      __m128 y = *((__m128 *)(src1 + c));
      __m128 cr = *((__m128 *)(src3 + c));
      __m128 red = _mm_mul_ps(cr,cr_fact_r);
      *((__m128 *)(src1 + c)) = _mm_add_ps(red,y); // Add in luminance to get red & save
      __m128 green = _mm_mul_ps(cr,neg_cr_fact_g);
      green = _mm_add_ps(green,y); // Y + scaled CR forms most of green
      __m128 cb = *((__m128 *)(src2 + c));
      __m128 blue = _mm_mul_ps(cb,cb_fact_b);
      *((__m128 *)(src3 + c)) = _mm_add_ps(blue,y); // Add in luminance to get blue & save
      cb = _mm_mul_ps(cb,neg_cb_fact_g);
      *((__m128 *)(src2 + c)) = _mm_add_ps(green,cb); // Complete and save the green channel
    }
}
#  define SSE2_SET_YCC_TO_RGB_IRREV32(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_ycc_to_rgb_irrev32;
#else // !KDU_NO_SSE
#  define SSE2_SET_YCC_TO_RGB_IRREV32(_tgt)
#endif

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_IRREV32(_tgt) \
  { \
    SSE2_SET_YCC_TO_RGB_IRREV32(_tgt); \
    AVX_SET_YCC_TO_RGB_IRREV32(_tgt); \
  }

/*****************************************************************************/
/* STATIC                   ..._rgb_to_ycc_rev16                             */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_rgb_to_ycc_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                        int samples)
{
  for (int c=0; c < samples; c += 8)
    {
      __m128i red = *((__m128i *)(src1 + c));
      __m128i green = *((__m128i *)(src2 + c));
      __m128i blue = *((__m128i *)(src3 + c));
      __m128i y = _mm_adds_epi16(red,blue);
      y = _mm_adds_epi16(y,green);
      y = _mm_adds_epi16(y,green); // Now have 2*G + R + B
      *((__m128i *)(src1 + c)) = _mm_srai_epi16(y,2); // Save Y = (2*G + R + B)>>2
      *((__m128i *)(src2 + c)) = _mm_subs_epi16(blue,green); // Save Db = B-G
      *((__m128i *)(src3 + c)) = _mm_subs_epi16(red,green); // Save Dr = R-G
    }
}
#  define SSE2_SET_RGB_TO_YCC_REV16(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_rgb_to_ycc_rev16;
#else // !KDU_NO_SSE
#  define SSE2_SET_RGB_TO_YCC_REV16(_tgt)
#endif

#if ((!defined KDU_NO_MMX64) && (KDU_MIN_MMX_LEVEL < 2))
static void
  mmx_rgb_to_ycc_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                       int samples)
{
  samples = (samples+3)>>2;
  __m64 *sp1 = (__m64 *) src1;
  __m64 *sp2 = (__m64 *) src2;
  __m64 *sp3 = (__m64 *) src3;
  for (int c=0; c < samples; c++)
    {
      __m64 red = sp1[c];
      __m64 green = sp2[c];
      __m64 blue = sp3[c];
      __m64 y = _mm_adds_pi16(red,blue);
      y = _mm_adds_pi16(y,green);
      y = _mm_adds_pi16(y,green); // Now have 2*G + R + B
      sp1[c] = _mm_srai_pi16(y,2); // Save Y = (2*G + R + B)>>2
      sp2[c] = _mm_subs_pi16(blue,green); // Save Db = B-G
      sp3[c] = _mm_subs_pi16(red,green); // Save Dr = R-G
  }
  _mm_empty(); // Clear MMX registers for use by FPU
}
#  define MMX_SET_RGB_TO_YCC_REV16(_tgt) \
          if (kdu_get_mmx_level() >= 1) _tgt=mmx_rgb_to_ycc_rev16;
#else // !KDU_NO_MMX64
#  define MMX_SET_RGB_TO_YCC_REV16(_tgt)
#endif

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_REV16(_tgt) \
  { \
    MMX_SET_RGB_TO_YCC_REV16(_tgt); \
    SSE2_SET_RGB_TO_YCC_REV16(_tgt); \
  }

/*****************************************************************************/
/* STATIC                   ..._rgb_to_ycc_rev32                             */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_rgb_to_ycc_rev32(kdu_int32 *src1, kdu_int32 *src2, kdu_int32 *src3,
                        int samples)
{
  for (int c=0; c < samples; c += 8)
    { // Slightly unrolled loop exploits fact that 32-bit sample buffers
      // are allocated in 32-byte chunks
      __m128i red1 = *((__m128i *)(src1 + c));
      __m128i green1 = *((__m128i *)(src2 + c));
      __m128i blue1 = *((__m128i *)(src3 + c));
      __m128i y = _mm_add_epi32(red1,blue1);
      y = _mm_add_epi32(y,green1);
      y = _mm_add_epi32(y,green1); // Now have 2*G + R + B
      *((__m128i *)(src1 + c)) = _mm_srai_epi32(y,2); // Save Y = (2*G + R + B)>>2
      *((__m128i *)(src2 + c)) = _mm_sub_epi32(blue1,green1); // Save Db = B-G
      *((__m128i *)(src3 + c)) = _mm_sub_epi32(red1,green1); // Save Dr = R-G
          
      __m128i red2 = *((__m128i *)(src1 + c + 4));
      __m128i green2 = *((__m128i *)(src2 + c + 4));
      __m128i blue2 = *((__m128i *)(src3 + c + 4));
      y = _mm_add_epi32(red2,blue2);
      y = _mm_add_epi32(y,green2);
      y = _mm_add_epi32(y,green2);
      *((__m128i *)(src1 + c + 4)) = _mm_srai_epi32(y,2);
      *((__m128i *)(src2 + c + 4)) = _mm_sub_epi32(blue2,green2);
      *((__m128i *)(src3 + c + 4)) = _mm_sub_epi32(red2,green2);
    }
}
#  define SSE2_SET_RGB_TO_YCC_REV32(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_rgb_to_ycc_rev32;
#else // !KDU_NO_SSE
#  define SSE2_SET_RGB_TO_YCC_REV32(_tgt)
#endif

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_REV32(_tgt) \
  { \
    SSE2_SET_RGB_TO_YCC_REV32(_tgt); \
  }

/*****************************************************************************/
/* STATIC                   ..._ycc_to_rgb_rev16                             */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_ycc_to_rgb_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                        int samples)
{
  for (int c=0; c < samples; c += 8)
    {
      __m128i db = *((__m128i *)(src2 + c));
      __m128i dr = *((__m128i *)(src3 + c));
      __m128i y = *((__m128i *)(src1 + c));
      __m128i tmp = _mm_adds_epi16(db,dr);
      tmp = _mm_srai_epi16(tmp,2); // Forms (Db+Dr)>>2
      __m128i green = _mm_subs_epi16(y,tmp);
      *((__m128i *)(src2 + c)) = green;
      *((__m128i *)(src1 + c)) = _mm_adds_epi16(dr,green);
      *((__m128i *)(src3 + c)) = _mm_adds_epi16(db,green);
    }
}
#  define SSE2_SET_YCC_TO_RGB_REV16(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_ycc_to_rgb_rev16;
#else // !KDU_NO_SSE
#  define SSE2_SET_YCC_TO_RGB_REV16(_tgt)
#endif

#if ((!defined KDU_NO_MMX64) && (KDU_MIN_MMX_LEVEL < 2))
static void
  mmx_ycc_to_rgb_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                       int samples)
{
  __m64 *sp1 = (__m64 *) src1;
  __m64 *sp2 = (__m64 *) src2;
  __m64 *sp3 = (__m64 *) src3;
  samples = (samples+3)>>2;
  for (int c=0; c < samples; c++)
    {
      __m64 db = sp2[c];
      __m64 dr = sp3[c];
      __m64 y = sp1[c];
      __m64 tmp = _mm_adds_pi16(db,dr);
      tmp = _mm_srai_pi16(tmp,2); // Forms (Db+Dr)>>2
      __m64 green = _mm_subs_pi16(y,tmp);
      sp2[c] = green;
      sp1[c] = _mm_adds_pi16(dr,green);
      sp3[c] = _mm_adds_pi16(db,green);
    }
  _mm_empty(); // Clear MMX registers for use by FPU
}
#  define MMX_SET_YCC_TO_RGB_REV16(_tgt) \
          if (kdu_get_mmx_level() >= 1) _tgt=mmx_ycc_to_rgb_rev16;
#else // !KDU_NO_MMX64
#  define MMX_SET_YCC_TO_RGB_REV16(_tgt)
#endif

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_REV16(_tgt) \
  { \
    MMX_SET_YCC_TO_RGB_REV16(_tgt); \
    SSE2_SET_YCC_TO_RGB_REV16(_tgt); \
  }

/*****************************************************************************/
/* STATIC                   ..._ycc_to_rgb_rev32                             */
/*****************************************************************************/

#ifndef KDU_NO_SSE
static void
  sse2_ycc_to_rgb_rev32(kdu_int32 *src1, kdu_int32 *src2, kdu_int32 *src3,
                        int samples)
{
  for (int c=0; c < samples; c += 8)
    { // Slightly unrolled loop exploits fact that 32-bit sample buffers
      // are allocated in 32-byte chunks
      __m128i db1 = *((__m128i *)(src2 + c));
      __m128i dr1 = *((__m128i *)(src3 + c));
      __m128i y = *((__m128i *)(src1 + c));
      __m128i tmp = _mm_add_epi32(db1,dr1);
      tmp = _mm_srai_epi32(tmp,2); // Forms (Db+Dr)>>2
      __m128i green = _mm_sub_epi32(y,tmp);
      *((__m128i *)(src2 + c)) = green;
      *((__m128i *)(src1 + c)) = _mm_add_epi32(dr1,green);
      *((__m128i *)(src3 + c)) = _mm_add_epi32(db1,green);

      __m128i db2 = *((__m128i *)(src2 + c + 4));
      __m128i dr2 = *((__m128i *)(src3 + c + 4));
      y = *((__m128i *)(src1 + c + 4));
      tmp = _mm_add_epi32(db2,dr2);
      tmp = _mm_srai_epi32(tmp,2);
      green = _mm_sub_epi32(y,tmp);
      *((__m128i *)(src2 + c + 4)) = green;
      *((__m128i *)(src1 + c + 4)) = _mm_add_epi32(dr2,green);
      *((__m128i *)(src3 + c + 4)) = _mm_add_epi32(db2,green);
    }
}
#  define SSE2_SET_YCC_TO_RGB_REV32(_tgt) \
          if (kdu_get_mmx_level() >= 2) _tgt=sse2_ycc_to_rgb_rev32;
#else // !KDU_NO_SSE
#  define SSE2_SET_YCC_TO_RGB_REV32(_tgt)
#endif

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_REV32(_tgt) \
  { \
    SSE2_SET_YCC_TO_RGB_REV32(_tgt); \
  }

#endif // X86_COLOUR_LOCAL_H
