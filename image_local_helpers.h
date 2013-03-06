/*****************************************************************************/
//  
//  @file: image_local_helpers.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Sean Peters
//  @date 01/03/13.
//  @brief These helper methods are defined within apps in the kakadu library. They
//  are required by code within are classes.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/*****************************************************************************/

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
inline void
  eat_white_and_comments(FILE *in);
    
inline void
  from_little_endian(kdu_int32 * words, int num_words);

void
  to_little_endian(kdu_int32 * words, int num_words);
