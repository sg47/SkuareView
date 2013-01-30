// TODO: document
/*****************************************************************************/
//  
//  @file: fits_local.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Sean Peters
//  @date 29/07/12.
//  @brief The The file contains the definitions of types and classes
//  @brief for the FITS image format.
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
