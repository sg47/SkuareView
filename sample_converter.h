/*****************************************************************************/
//  
//  @file: sample_converters.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Sean Peters
//  @date 01/03/13.
//  @brief These helper methods are defined within apps in the kakadu library. They
//  are required by code within are classes.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/*****************************************************************************/

// System includes
#include <iostream>
#include <string.h>
#include <math.h>
#include <assert.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
#include "kdu_args.h"
// Image includes
#include "kdu_image.h"
// IO includes
#include "kdu_file_io.h"
#include "image_local.h"

void
  to_little_endian(kdu_int32 * words, int num_words);

inline void
  from_little_endian(kdu_int32 * words, int num_words);
  
inline void
  eat_white_and_comments(FILE *in);

void
  convert_words_to_floats(kdu_byte *src, kdu_sample32 *dest, int num,
                          int precision, bool is_signed, int sample_bytes,
                          bool littlendian, int inter_sample_bytes=0);

void
  convert_words_to_fixpoint(kdu_byte *src, kdu_sample16 *dest, int num,
                            int precision, bool is_signed, int sample_bytes,
                            bool littlendian, int inter_sample_bytes=0);
void
  convert_words_to_ints(kdu_byte *src, kdu_sample32 *dest, int num,
                        int precision, bool is_signed, int sample_bytes,
                        bool littlendian, int inter_sample_bytes=0);
void
  convert_words_to_shorts(kdu_byte *src, kdu_sample16 *dest, int num,
                         int precision, bool is_signed, int sample_bytes,
                         bool littlendian, int inter_sample_bytes=0);
void
  convert_floats_to_ints(kdu_byte *src, kdu_sample32 *dest,  int num,
                         int precision, bool is_signed,
                         double minval, double maxval, int sample_bytes,
                         bool littlendian, int inter_sample_bytes);
void
  convert_floats_to_floats(kdu_byte *src, kdu_sample32 *dest,  int num,
                           int precision, bool is_signed,
                           double minval, double maxval, int sample_bytes,
                           bool littlendian, int inter_sample_bytes);

  /* If working with an absolute representation, and `align_lsbs' is true,
     this function truncates the samples to fit inside the range -2^{P-1} to
     2^{P-1}-1, where P is the value of `forced_prec'.  If `is_signed' is
     false, the original samples are offset by 2^{I-1} - 2^{P-1} before the
     truncation is applied, where I is the value of `initial_prec'.
        Considering absolute integers again, if `align_lsbs' is true, the
     function scales the input values by 2^{P-I}, rounding the result to the
     nearest integer if P < I.  The value of `is_signed' is irrelevant in this
     case.
        If working with a fixed-point or floating point representation, and
     `align_lsbs' is true, the function scales the data by
     2^{I-P} clipping upscaled values, as appropriate.  If `is_signed' is
     false, the original samples are first offset by a value of
     (2^{I-1} - 2^{P-1}) / 2^I  (for floating-point samples) or
     (2^{I-1} - 2^{P-1}) / 2^I * 2^{KDU_FIX_POINT} (for fixed-point samples).
        Considering floating-point and fixed-point samples again, if
     `align_lsbs' is false, the function rounds the original sample values to
     the nearest multiple of 2^{-P} (for floating-point) or
     2^{KDU_FIX_POINT-P} (for fixed-point).  The value of `is_signed' is
     irrelevant in this case.
  */
void
  force_sample_precision(kdu_line_buf &line, int forced_prec,
                         bool align_lsbs, int initial_prec, bool is_signed);
