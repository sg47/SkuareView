// HEADER COMMENT

#include "sample_converter.h"

/*****************************************************************************/
/*                              to_little_endian                             */
/*****************************************************************************/

void
  to_little_endian(kdu_int32 * words, int num_words)
{
  kdu_int32 test = 1;
  kdu_byte *first_byte = (kdu_byte *) &test;
  if (*first_byte)
    return; // Machine uses little-endian architecture already.
  kdu_int32 tmp;
  for (; num_words--; words++)
    {
      tmp = *words;
      *words = ((tmp >> 24) & 0x000000FF) +
               ((tmp >> 8)  & 0x0000FF00) +
               ((tmp << 8)  & 0x00FF0000) +
               ((tmp << 24) & 0xFF000000);
    }
}

/*****************************************************************************/
/*                             from_little_endian                            */
/*****************************************************************************/

inline void
  from_little_endian(kdu_int32 * words, int num_words)
{
  to_little_endian(words,num_words);
}

/*****************************************************************************/
/*                           eat_white_and_comments                          */
/*****************************************************************************/

inline void
  eat_white_and_comments(FILE *in)
{
  int ch;
  bool in_comment;

  in_comment = false;
  while ((ch = getc(in)) != EOF)
    if (ch == '#')
      in_comment = true;
    else if (ch == '\n')
      in_comment = false;
    else if ((!in_comment) && (ch != ' ') && (ch != '\t') && (ch != '\r'))
      {
        ungetc(ch,in);
        return;
      }
}

/*****************************************************************************/
/*                           convert_words_to_floats                         */
/*****************************************************************************/

void
  convert_words_to_floats(kdu_byte *src, kdu_sample32 *dest, int num,
                          int precision, bool is_signed, int sample_bytes,
                          bool littlendian, int inter_sample_bytes)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  float scale;
  if (precision < 30)
    scale = (float)(1<<precision);
  else
    scale = ((float)(1<<30)) * ((float)(1<<(precision-30)));
  scale = 1.0F / scale;
  
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    { 
      for (; num > 0; num--, dest++, src+=inter_sample_bytes)
        {
          val = src[0];
          val += offset;  val &= mask;  val -= centre;
          dest->fval = ((float) val) * scale;
        }
    }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1]; val = (val<<8) + src[2];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[2]; val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val = (val<<8) + src[2]; val = (val<<8) + src[3];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[3]; val = (val<<8) + src[2];
            val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->fval = ((float) val) * scale;
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/*                          convert_words_to_fixpoint                        */
/*****************************************************************************/

void
  convert_words_to_fixpoint(kdu_byte *src, kdu_sample16 *dest, int num,
                            int precision, bool is_signed, int sample_bytes,
                            bool littlendian, int inter_sample_bytes)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 upshift = KDU_FIX_POINT-precision;
  if (upshift < 0)
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = src[0];
        val += offset;  val &= mask;  val -= centre;
        dest->ival = (kdu_int16)(val<<upshift);
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16)(val<<upshift);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16)(val<<upshift);
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
}

/*****************************************************************************/
/*                            convert_words_to_ints                          */
/*****************************************************************************/

void
  convert_words_to_ints(kdu_byte *src, kdu_sample32 *dest, int num,
                        int precision, bool is_signed, int sample_bytes,
                        bool littlendian, int inter_sample_bytes)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = *src;
        val += offset;  val &= mask;  val -= centre;
        dest->ival = val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else if (sample_bytes == 3)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1]; val = (val<<8) + src[2];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[2]; val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else if (sample_bytes == 4)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val = (val<<8) + src[2]; val = (val<<8) + src[3];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[3]; val = (val<<8) + src[2];
            val = (val<<8) + src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = val;
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/*                          convert_words_to_shorts                          */
/*****************************************************************************/

void
  convert_words_to_shorts(kdu_byte *src, kdu_sample16 *dest, int num,
                          int precision, bool is_signed, int sample_bytes,
                          bool littlendian, int inter_sample_bytes)
{
  if (inter_sample_bytes == 0)
    inter_sample_bytes = sample_bytes;
  kdu_int32 centre = 1<<(precision-1);
  kdu_int32 offset = (is_signed)?centre:0;
  kdu_int32 mask = ~((-1)<<precision);
  kdu_int32 val;

  if (sample_bytes == 1)
    for (; num > 0; num--, dest++, src+=inter_sample_bytes)
      {
        val = src[0];
        val += offset;  val &= mask;  val -= centre;
        dest->ival = (kdu_int16) val;
      }
  else if (sample_bytes == 2)
    {
      if (!littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[0]; val = (val<<8) + src[1];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16) val;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            val = src[1]; val = (val<<8) + src[0];
            val += offset;  val &= mask;  val -= centre;
            dest->ival = (kdu_int16) val;
          }
    }
  else
    { kdu_error e; e << "Cannot use 16-bit representation with high "
      "bit-depth data"; }
}

/*****************************************************************************/
/*                          convert_floats_to_ints                           */
/*****************************************************************************/

void
  convert_floats_to_ints(kdu_byte *src, kdu_sample32 *dest,  int num,
                         int precision, bool is_signed,
                         double minval, double maxval, int sample_bytes,
                         bool littlendian, int inter_sample_bytes)
{
  int test = 1;
  bool native_littlendian = (((kdu_byte *) &test)[0] != 0);

  double scale, offset=0.0;
  double limmin=-0.75, limmax=0.75;
  if (is_signed)
    scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
  else
    {
      scale = 1.0 / maxval;
      offset = -0.5;
    }
  scale *= (double)((((kdu_long) 1) << precision)-1);
  offset *= (double)(((kdu_long) 1) << precision);
  limmin *= (double)(((kdu_long) 1) << precision);
  limmax *= (double)(((kdu_long) 1) << precision);
  offset += 0.5; // For rounding

  if (sample_bytes == 4)
    { // Transfer floats to ints
      union {
          float fbuf_val;
          kdu_byte fbuf[4];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) fval;
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[3]=src[0]; fbuf[2]=src[1]; fbuf[1]=src[2]; fbuf[0]=src[3];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) floor(fval);
          }
    }
  else if (sample_bytes == 8)
    { // Transfer doubles to ints, with some scaling
      union {
          double fbuf_val;
          kdu_byte fbuf[8];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            fbuf[4]=src[4]; fbuf[5]=src[5]; fbuf[6]=src[6]; fbuf[7]=src[7];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) floor(fval);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[7]=src[0]; fbuf[6]=src[1]; fbuf[5]=src[2]; fbuf[4]=src[3];
            fbuf[3]=src[4]; fbuf[2]=src[5]; fbuf[1]=src[6]; fbuf[0]=src[7];
            double fval = fbuf_val * scale + offset;
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest->ival = (kdu_int32) floor(fval);
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/*                         convert_floats_to_floats                          */
/*****************************************************************************/

void
  convert_floats_to_floats(kdu_byte *src, kdu_sample32 *dest,  int num,
                           int precision, bool is_signed,
                           double minval, double maxval, int sample_bytes,
                           bool littlendian, int inter_sample_bytes)
{
  int test = 1;
  bool native_littlendian = (((kdu_byte *) &test)[0] != 0);

  double scale, offset=0.0;
  if (is_signed)
    scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
  else
    {
      scale = 1.0 / maxval;
      offset = -0.5;
    }
  scale *= (1.0 - 1.0 / (double)(((kdu_long) 1) << precision));

  if (sample_bytes == 4)
    { // Transfer floats to floats, with some scaling
      union {
          float fbuf_val;
          kdu_byte fbuf[4];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[3]=src[0]; fbuf[2]=src[1]; fbuf[1]=src[2]; fbuf[0]=src[3];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
    }
  else if (sample_bytes == 8)
    { // Transfer doubles to floats, with some scaling
      union {
          double fbuf_val;
          kdu_byte fbuf[8];
        };
      if (littlendian == native_littlendian)
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[0]=src[0]; fbuf[1]=src[1]; fbuf[2]=src[2]; fbuf[3]=src[3];
            fbuf[4]=src[4]; fbuf[5]=src[5]; fbuf[6]=src[6]; fbuf[7]=src[7];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
      else
        for (; num > 0; num--, dest++, src+=inter_sample_bytes)
          {
            fbuf[7]=src[0]; fbuf[6]=src[1]; fbuf[5]=src[2]; fbuf[4]=src[3];
            fbuf[3]=src[4]; fbuf[2]=src[5]; fbuf[1]=src[6]; fbuf[0]=src[7];
            dest->fval = (float)(fbuf_val * scale + offset);
          }
    }
  else
    assert(0);
}

/*****************************************************************************/
/*                           force_sample_precision                          */
/*****************************************************************************/

void
  force_sample_precision(kdu_line_buf &line, int forced_prec,
                         bool align_lsbs, int initial_prec, bool is_signed)
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
{
  assert(initial_prec > 0);
  if (initial_prec == forced_prec)
    return;
  int n = line.get_width();
  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *sp = line.get_buf32();
      if (line.is_absolute())
        {
          if (forced_prec >= initial_prec)
            {
              if (align_lsbs)
                { // No need to truncate or scale
                  if (is_signed)
                    return; // Nothing to do at all
                  kdu_int32 offset =
                    ((1<<initial_prec)>>1)-((1<<forced_prec)>>1);
                  for (; n > 0; n--, sp++)
                    sp->ival += offset;
                }
              else
                { // No need to round, or offset
                  int shift = forced_prec - initial_prec;
                  for (; n > 0; n--, sp++)
                    sp->ival <<= shift;
                }
            }
          else
            {
              if (align_lsbs)
                { // Need to truncate and offset (if not signed)
                  kdu_int32 offset = 0;
                  if (!is_signed)
                    offset = ((1<<initial_prec)>>1)-((1<<forced_prec)>>1);
                  kdu_int32 min_val = -(1<<(forced_prec-1));
                  kdu_int32 max_val = -min_val-1;
                  min_val -= offset;  max_val -= offset;
                  for (; n > 0; n--, sp++)
                    {
                      int val = sp->ival;
                      if (val < min_val)
                        val = min_val;
                      else if (val > max_val)
                        val = max_val;
                      sp->ival = val + offset;
                    }
                }
              else
                { // Need to downshift, with rounding
                  int shift = initial_prec - forced_prec;
                  kdu_int32 offset = (1<<shift)>>1;
                  for (; n > 0; n--, sp++)
                    sp->ival = (sp->ival + offset) >> shift;
                }
            }
        }
      else
        { // 32-bit floating point values
          if (align_lsbs)
            { // Need to scale and perhaps offset values
              float scale =
                ((float)(1<<initial_prec)) / ((float)(1<<forced_prec));
              float offset = 0.0F;
              if (!is_signed)
                offset = 0.5F*scale - 0.5F;
              float max_val = 0.5F - 1.0F / (float)(1<<forced_prec);
              for (; n > 0; n--, sp++)
                {
                  float val = sp->fval * scale + offset;
                  if (val < -0.5F)
                    sp->fval = -0.5F;
                  else if (val > max_val)
                    sp->fval = max_val;
                  else
                    sp->fval = val;
                }
            }
          else if (forced_prec < initial_prec)
            { // Need to round input values
              float pre_scale = (float)(1<<forced_prec);
              float post_scale = 1.0F / pre_scale;
              for (; n > 0; n--, sp++)
                {
                  float val = sp->fval * pre_scale;
                  kdu_int32 ival = (val<0.0F)?
                    (-(kdu_int32)(0.5F-val)):((kdu_int32)(0.5F+val));
                  sp->fval = post_scale * ival;
                }
            }
        }
    }
  else
    {
      kdu_sample16 *sp = line.get_buf16();
      if (line.is_absolute())
        {
          if (forced_prec >= initial_prec)
            {
              if (align_lsbs)
                { // No need to truncate or scale
                  if (is_signed)
                    return;
                  kdu_int16 offset = (kdu_int16)
                    (((1<<initial_prec)>>1)-((1<<forced_prec)>>1));
                  for (; n > 0; n--, sp++)
                    sp->ival += offset;
                }
              else
                { // No need to round, or offset
                  int shift = forced_prec - initial_prec;
                  for (; n > 0; n--, sp++)
                    sp->ival <<= shift;
                }
            }
          else
            {
              if (align_lsbs)
                { // Need to truncate and offset (if not signed)
                  kdu_int16 offset = 0;
                  if (!is_signed)
                    offset = (kdu_int16)
                      (((1<<initial_prec)>>1)-((1<<forced_prec)>>1));
                  kdu_int16 min_val = (kdu_int16) -(1<<(forced_prec-1));
                  kdu_int16 max_val = -min_val-1;
                  min_val -= offset;  max_val -= offset;
                  for (; n > 0; n--, sp++)
                    {
                      kdu_int16 val = sp->ival;
                      if (val < min_val)
                        val = min_val;
                      else if (val > max_val)
                        val = max_val;
                      sp->ival = val + offset;
                    }
                }
              else
                { // Need to downshift, with rounding
                  int shift = initial_prec - forced_prec;
                  kdu_int16 offset = (kdu_int16)((1<<shift)>>1);
                  for (; n > 0; n--, sp++)
                    sp->ival = (sp->ival + offset) >> shift;
                }
            }
        }
      else if (forced_prec < initial_prec)
        {
          if (align_lsbs)
            { // Need to scale and perhaps offset input samples
              int upshift = initial_prec - forced_prec;
              kdu_int16 min_val=0, max_val=0;
              kdu_int16 offset = 0;
              if (upshift < KDU_FIX_POINT)
                {
                  min_val = (kdu_int16) -(1<<(KDU_FIX_POINT-upshift-1));
                  max_val = -min_val;
                  if (initial_prec <= KDU_FIX_POINT)
                    max_val -= (1<<(KDU_FIX_POINT-initial_prec));
                  if (!is_signed)
                    offset = (kdu_int16)((1<<(KDU_FIX_POINT-1)) -
                                         (1<<(KDU_FIX_POINT-upshift-1)));
                  min_val -= offset;  max_val -= offset;
                }
              for (; n > 0; n--, sp++)
                {
                  kdu_int16 val = sp->ival;
                  if (val < min_val)
                    val = min_val;
                  else if (val > max_val)
                    val = max_val;
                  sp->ival = (val + offset) << upshift;
                }
            }
          else if (forced_prec < KDU_FIX_POINT)
            { // Need to round to a multiple of 2^{KDU_FIX_POINT-forced_prec}
              int shift = KDU_FIX_POINT - forced_prec;
              kdu_int16 mask_val = (kdu_int16)((-1)<<shift);
              kdu_int16 offset = (kdu_int16)((1<<shift)>>1);
              for (; n > 0; n--, sp++)
                sp->ival = (sp->ival + offset) & mask_val;
            }
        }
      else
        {
          if (align_lsbs)
            { // Need to scale and perhaps offset
              int downshift = forced_prec - initial_prec;
              kdu_int32 offset = (kdu_int32)((1<<downshift)>>1);
              if (!is_signed)
                offset += ((1<<(KDU_FIX_POINT-1)) -
                           (1<<(KDU_FIX_POINT+downshift-1)));
              for (; n > 0; n--, sp++)
                {
                  int val = sp->ival;
                  sp->ival = (kdu_int16)((val + offset) >> downshift);
                }
            }
        }
    }
}

/*****************************************************************************/
/*                                invert_line                                */
/*****************************************************************************/

void
  invert_line(kdu_line_buf &line, int precision)
  /* Swaps the roles of the minimum and maximum sample intensities.
     `precision' is required to do this correctly, since the minimum and
     maximum values associated with a `precision'-bit representation do
     not exactly sum to 0. */
{
  int n = line.get_width();
  if (line.get_buf32() != NULL)
    {
      kdu_sample32 *sp = line.get_buf32();
      if (line.is_absolute())
        for (; n > 0; n--, sp++)
          sp->ival = -1-sp->ival;
      else
        {
          float offset = -1.0F / (1<<precision);
          for (; n > 0; n--, sp++)
            sp->fval = offset-sp->fval;
        }
    }
  else
    {
      kdu_sample16 *sp = line.get_buf16();
      if (line.is_absolute())
        for (; n > 0; n--, sp++)
          sp->ival = -1-sp->ival;
      else
        {
          kdu_int16 offset = -(1 + (((1<<KDU_FIX_POINT)-1)>>precision));
          for (; n > 0; n--, sp++)
            sp->ival = offset-sp->ival;
        }
    }
}
