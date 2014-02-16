/*****************************************************************************/
// File: gcc_colour_altivec_local.h [scope = CORESYS/TRANSFORMS]
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
   Provides Altivec implementations of the colour transformations: both
reversible (RCT) and irreversible (ICT = RGB to YCbCr).
******************************************************************************/
#ifndef GCC_COLOUR_ALTIVEC_LOCAL_H
#define GCC_COLOUR_ALTIVEC_LOCAL_H

/* ========================================================================= */
/*                        Now for the Altivec functions                      */
/* ========================================================================= */

/*****************************************************************************
    ASSUMPTIONS:
    
    All buffers passed in will be 16-byte aligned.
    All operation sizes can safely be rounded up to a multiple of 16 bytes
    (i.e. 8 samples).  Violating either of these assumtions WILL cause this
    code to generate incorrect results.
 *****************************************************************************/

/*****************************************************************************
    Note on coding style:
    
    The way I've chosen to write some of these expressions may look awful
    at first glance, but there's a reason behind it.
    
    Writing the altivec intrinsics as a series of nested expressions (instead
    of one per statement with explicit temporary variables) makes a number of
    things easier for the compiler:
    
    - vector register assignment and proper scoping of temporary registers
    - instruction scheduling (more latitude for rearranging instructions)
    - common subexpression elimination (although I've already tried to do this
        manually in most cases)
    
    In particular, declaring explicit temporaries tends to use up registers
    unnecessarily (if you create lots of temporaries) and/or create false 
    data dependencies (if you only have a few but re-use them).  Using nested
    expressions instead gives the compiler as much information as possible
    about the scope of temporary data, allowing it to do optimal register
    assignment and avoid data dependencies.  A really smart compiler wouldn't
    need this assistance...
 *****************************************************************************/
 
/*****************************************************************************/
/* STATIC                  ..._rgb_to_ycc_irrev16                            */
/*****************************************************************************/

static void
  vec_rgb_to_ycc_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                         int samples)
{
  if (samples <= 0)
    return;

    // Only load one vector of constants from memory, build the rest.
    vector signed short constants = (vector signed short) 
      (
        // All these factors need to be in the range of [-0.5, 0.5) for correct
        // results.  This is because we are using normalized signed 16-bit
        // integer maths and the available high-half multiply instruction does
        // an implicit >> 15 and signed saturate.  We use non-saturating (i.e.
        // modulo) adds and subtracts to make up the difference.  Stare at the
        // documentation for vec_mradds long enough and this may begin to make
        // sense.
        (signed short)(0.299 * (1<<15)),     // alphaR
        (signed short)(0.114 * (1<<15)),     // alphaB
        (signed short)(0.4356659 * (1<<15)), // CBfact -- Actual value is 1-0.4356659
        (signed short)(0.2867332 * (1<<15)), // CRfact -- Actual value is 1-0.2867332
        (signed short)(0.413 * (1<<15)),     // alphaG -- actual value is 1-0.413 = 0.587
        0, 0, 0                           // Don't need the rest of the vector.
    );
    
    // Splat these outside the loop, since they will be used every iteration.
    vector signed short zero = vec_splat_s16(0);
    vector signed short alphaR = vec_splat(constants, 0);
    vector signed short alphaB = vec_splat(constants, 1);
    vector signed short CBfact = vec_splat(constants, 2);
    vector signed short CRfact = vec_splat(constants, 3);
    vector signed short alphaG = vec_splat(constants, 4);

    int count = (samples+7)>>3;
    
    // The compiler should do this loop using the count register; it
    // may unroll it if it's smart enough.
    for(; count; count--)
    {
        vector signed short inR = vec_ld(0, src1);
        vector signed short inG = vec_ld(0, src2);
        vector signed short inB = vec_ld(0, src3);

        // Don't use the fused add in vec_mradds; it's followed by a
        // signed saturate that may cause incorrect results.
        vector signed short outY = 
            vec_add
            (
                vec_add
                (
                    vec_mradds(inR, alphaR, zero),
                    vec_mradds(inB, alphaB, zero)
                ),
                vec_sub(inG, vec_mradds(inG, alphaG, zero))
            );
        
        vector signed short partialCb = vec_sub(inB, outY);

        vector signed short partialCr = vec_sub(inR, outY);

        vector signed short outCb = 
            vec_sub
            (
                partialCb,
                vec_mradds(CBfact, partialCb, zero)
            );

        vector signed short outCr = 
            vec_sub
            (
                partialCr,
                vec_mradds(CRfact, partialCr, zero)
            );

        vec_st(outY, 0, src1);
        vec_st(outCb, 0, src2);
        vec_st(outCr, 0, src3);
        
        src1 += 8;
        src2 += 8;
        src3 += 8;
    }
}

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_IRREV16(_tgt) \
  { \
    if (kdu_get_altivec_exists()) _tgt=vec_rgb_to_ycc_irrev16; \
  }

/*****************************************************************************/
/* STATIC                  ..._ycc_to_rgb_irrev16                            */
/*****************************************************************************/

static void
  vec_ycc_to_rgb_irrev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                         int samples)
{
  if (samples <= 0)
    return;

  // Only load one vector of constants from memory, build the rest.
  vector signed short constants = (vector signed short) 
    (
        // All these factors need to be in the range of [-0.5, 0.5) for correct
        // results.  This is because we are using normalized signed 16-bit
        // integer maths and the available high-half multiply instruction does
        // an implicit >> 15 and signed saturate.  We use non-saturating (i.e.
        // modulo) adds and subtracts to make up the difference.  Stare at the
        // documentation for vec_mradds long enough and this may begin to make
        // sense.
        (signed short)(0.402 * (1<<15)),     // q_CRfactR -- Actual factor is 1.402
        (signed short)(-0.228 * (1<<15)),    // q_CBfactB -- Actual factor is 1.772
        (signed short)(0.285864 * (1<<15)),  // q_CRfactG -- Actual factor is -0.714136
        (signed short)(-0.344136 * (1<<15)), // q_CBfactG -- Actual factor is -0.344136
        0, 0, 0, 0                        // Don't need the rest of the vector.
    );

  // Splat these outside the loop, since they will be used every iteration.
  vector signed short zero = vec_splat_s16(0);
  vector signed short CRfactR = vec_splat(constants, 0);
  vector signed short CBfactB = vec_splat(constants, 1);
  vector signed short CRfactG = vec_splat(constants, 2);
  vector signed short CBfactG = vec_splat(constants, 3);

  int count = (samples+7)>>3;
    
  // The compiler should do this loop using the count register;
  // it may unroll it if it's smart enough.
  for(; count; count--)
    {
        vector signed short inY  = vec_ld(0, src1);
        vector signed short inCb = vec_ld(0, src2);
        vector signed short inCr = vec_ld(0, src3);

        // Don't use the fused add in vec_mradds; it's followed by a signed
        // saturate that may cause incorrect results.
        vector signed short outR = 
            vec_add
            (
                inY, 
                vec_add
                (
                    inCr, 
                    vec_mradds(inCr, CRfactR, zero)
                )
            );
        vector signed short outB = 
            vec_add
            (
                inY, 
                vec_add
                (
                    inCb, 
                    vec_add
                    (
                        inCb, 
                        vec_mradds(inCb, CBfactB, zero)
                    )
                )
            );
        vector signed short outG = 
            vec_add
            (
                vec_add
                (
                    inY,
                    vec_sub
                    (
                        vec_mradds(inCr, CRfactG,zero),
                        inCr 
                    )
                ),
                vec_mradds(inCb, CBfactG, zero)
            );
        
        vec_st(outR, 0, src1);
        vec_st(outG, 0, src2);
        vec_st(outB, 0, src3);
        
        src1 += 8;
        src2 += 8;
        src3 += 8;
    }
}

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_IRREV16(_tgt) \
  { \
    if (kdu_get_altivec_exists()) _tgt=vec_ycc_to_rgb_irrev16; \
  }

/*****************************************************************************/
/* STATIC                   ..._rgb_to_ycc_rev16                             */
/*****************************************************************************/

static void
  vec_rgb_to_ycc_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                       int samples)
{
  if (samples <= 0)
    return;

  vector unsigned short two = vec_splat_u16(2);
  int count = (samples+7)>>3;
    
  // The compiler should do this loop using the count register.
  for(; count; count--)
    {
        vector signed short inR, inG, inB, outY, outCb, outCr;
        inR = vec_ld(0, src1);
        inG = vec_ld(0, src2);
        inB = vec_ld(0, src3);
        
        outY = vec_sra(vec_adds(vec_adds(inG, inG), vec_adds(inB, inR)), two);
        outCr = vec_subs(inR, inG);
        outCb = vec_subs(inB, inG);

        vec_st(outY, 0, src1);
        vec_st(outCb, 0, src2);
        vec_st(outCr, 0, src3);
        
        src1 += 8;
        src2 += 8;
        src3 += 8;
    }
}

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_REV16(_tgt) \
  { \
    if (kdu_get_altivec_exists()) _tgt=vec_rgb_to_ycc_rev16; \
  }

/*****************************************************************************/
/* STATIC                   ..._ycc_to_rgb_rev16                             */
/*****************************************************************************/

static void
  vec_ycc_to_rgb_rev16(kdu_int16 *src1, kdu_int16 *src2, kdu_int16 *src3,
                       int samples)
{
  if (samples <= 0)
    return;

  vector unsigned short two = vec_splat_u16(2);
  int count = (samples+7)>>3;
    
  // The compiler should do this loop using the count register.
  for(; count; count--)
    {
        vector signed short inY, inCb, inCr, outR, outG, outB;
        inY  = vec_ld(0, src1);
        inCb = vec_ld(0, src2);
        inCr = vec_ld(0, src3);
        
        outG = vec_sub(inY, vec_sra(vec_add(inCb, inCr), two));
        outR = vec_add(outG, inCr);
        outB = vec_add(outG, inCb);
        
        vec_st(outR, 0, src1);
        vec_st(outG, 0, src2);
        vec_st(outB, 0, src3);
        
        src1 += 8;
        src2 += 8;
        src3 += 8;
    }
}

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_REV16(_tgt) \
  { \
    if (kdu_get_altivec_exists()) _tgt=vec_ycc_to_rgb_rev16; \
  }

/*****************************************************************************/
/* STATIC                  ..._rgb_to_ycc_irrev32                            */
/*****************************************************************************/

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_IRREV32(_tgt)

/*****************************************************************************/
/* STATIC                   ..._rgb_to_ycc_rev32                             */
/*****************************************************************************/

#define KD_SET_SIMD_FUNC_RGB_TO_YCC_REV32(_tgt)

/*****************************************************************************/
/* STATIC                  ..._ycc_to_rgb_irrev32                            */
/*****************************************************************************/

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_IRREV32(_tgt)

/*****************************************************************************/
/* STATIC                   ..._ycc_to_rgb_rev32                             */
/*****************************************************************************/

#define KD_SET_SIMD_FUNC_YCC_TO_RGB_REV32(_tgt)

#endif // GCC_COLOUR_ALTIVEC_LOCAL_H
