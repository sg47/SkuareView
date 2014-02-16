/*****************************************************************************/
// File: x86_stripe_transfer_local.h [scope = APPS/SUPPORT]
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
   Finds SIMD implementations to accelerate the conversion and transfer of
data between the line buffers generated by `kdu_multi_synthesis' or
`kdu_multi_analysis' and the (possibly interleaved) application-supplied
sample buffers supplied to `kdu_stripe_decompressor::pull_stripe' or
`kdu_stripe_compressor::push_stripe'.  This file provides macros to
arbitrate the selection of suitable SIMD functions, if they exist.  The actual
SIMD functions themselves appear within "ssse3_stripe_transfer.cpp".
******************************************************************************/
#ifndef X86_STRIPE_TRANSFER_LOCAL_H
#define X86_STRIPE_TRANSFER_LOCAL_H

/* ========================================================================= */
/*               SIMD functions used by `kdu_stripe_compressor'              */
/* ========================================================================= */

#  define SSSE3_INT16_FROM_UINT8_ILV1(_func)
#  define SSSE3_INT16_FROM_UINT8_ILV3(_func)
#  define SSSE3_INT16_FROM_UINT8_ILV4(_func)

#  define SSSE3_FLOATS_FROM_UINT8_ILV1(_func)
#  define SSSE3_FLOATS_FROM_UINT8_ILV3(_func)
#  define SSSE3_FLOATS_FROM_UINT8_ILV4(_func)

#  define SSSE3_INT16_FROM_INT16_ILV1(_func)
#  define SSSE3_INT32_FROM_INT16_ILV1(_func)
#  define SSSE3_FLOATS_FROM_INT16_ILV1(_func)

#  define SSSE3_FLOATS_FROM_FLOATS_ILV1(_func)

#ifndef KDU_NO_SSSE3
//----------------------------------------------------------------------------
extern void ssse3_int16_from_uint8_ilv1(kdu_int16 **, kdu_byte *,
                                        int, int, int, bool, bool);
#undef SSSE3_INT16_FROM_UINT8_ILV1
#define SSSE3_INT16_FROM_UINT8_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_int16_from_uint8_ilv1
//----------------------------------------------------------------------------
extern void ssse3_int16_from_uint8_ilv3(kdu_int16 **, kdu_byte *,
                                        int, int, int, bool, bool);
#undef SSSE3_INT16_FROM_UINT8_ILV3
#define SSSE3_INT16_FROM_UINT8_ILV3(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_int16_from_uint8_ilv3
//----------------------------------------------------------------------------
extern void ssse3_int16_from_uint8_ilv4(kdu_int16 **, kdu_byte *,
                                        int, int, int, bool, bool);
#undef SSSE3_INT16_FROM_UINT8_ILV4
#define SSSE3_INT16_FROM_UINT8_ILV4(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_int16_from_uint8_ilv4
//----------------------------------------------------------------------------
extern void ssse3_floats_from_uint8_ilv1(float **, kdu_byte *,
                                            int, int, int, bool, bool);
#undef SSSE3_FLOATS_FROM_UINT8_ILV1
#define SSSE3_FLOATS_FROM_UINT8_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_floats_from_uint8_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_from_uint8_ilv3(float **, kdu_byte *,
                                         int, int, int, bool, bool);
#undef SSSE3_FLOATS_FROM_UINT8_ILV3
#define SSSE3_FLOATS_FROM_UINT8_ILV3(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_floats_from_uint8_ilv3
//----------------------------------------------------------------------------
extern void ssse3_floats_from_uint8_ilv4(float **, kdu_byte *,
                                         int, int, int, bool, bool);
#undef SSSE3_FLOATS_FROM_UINT8_ILV4
#define SSSE3_FLOATS_FROM_UINT8_ILV4(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_floats_from_uint8_ilv4
//----------------------------------------------------------------------------
extern void ssse3_int16_from_int16_ilv1(kdu_int16 **, kdu_int16 *,
                                        int, int, int, bool, bool);
#undef SSSE3_INT16_FROM_INT16_ILV1
#define SSSE3_INT16_FROM_INT16_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_int16_from_int16_ilv1
//----------------------------------------------------------------------------
extern void ssse3_int32_from_int16_ilv1(kdu_int32 **, kdu_int16 *,
                                        int, int, int, bool, bool);
#undef SSSE3_INT32_FROM_INT16_ILV1
#define SSSE3_INT32_FROM_INT16_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_int32_from_int16_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_from_int16_ilv1(float **, kdu_int16 *,
                                         int, int, int, bool, bool);
#undef SSSE3_FLOATS_FROM_INT16_ILV1
#define SSSE3_FLOATS_FROM_INT16_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_floats_from_int16_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_from_floats_ilv1(float **, float *,
                                          int, int, int, bool, bool);
#undef SSSE3_FLOATS_FROM_FLOATS_ILV1
#define SSSE3_FLOATS_FROM_FLOATS_ILV1(_func) \
  _func = (kdsc_simd_transfer_func) ssse3_floats_from_floats_ilv1
//----------------------------------------------------------------------------
#endif // !KDU_NO_SSSE3


#define KDSC_FIND_SIMD_TRANSFER_FUNC(_func, _buf_type, _shorts, \
                                _sample_gap, _prec, _tgt_prec, _absolute) \
  /* NB: last 3 args are identical to those passed to the transfer func. */ \
{ \
    _func = NULL; /* Until proven otherwise. */ \
    if ((kdu_mmx_level >= 4) && (_sample_gap == 1)) \
      { \
        if (_buf_type == KDSC_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                SSSE3_INT16_FROM_UINT8_ILV1(_func); \
              } \
            else if (!_absolute) \
              { /* Convert floating point to bytes. */ \
                SSSE3_FLOATS_FROM_UINT8_ILV1(_func); \
              } \
          } \
        else if (_buf_type == KDSC_BUF16) \
          { \
            if (_shorts) \
              { /* Convert words to words, allowing left or right shifts. */ \
                SSSE3_INT16_FROM_INT16_ILV1(_func); \
              } \
            else if (!_absolute) \
              { /* Convert floating point to words. */ \
                SSSE3_FLOATS_FROM_INT16_ILV1(_func); \
              } \
            else \
              { /* Convert 32-bit ints to words, with possible rightshift. */ \
                SSSE3_INT32_FROM_INT16_ILV1(_func); \
              } \
          } \
        else if (_buf_type == KDSC_BUF_FLOAT) \
          { \
            if (!_absolute) \
              SSSE3_FLOATS_FROM_FLOATS_ILV1(_func); \
          } \
      } \
    else if ((kdu_mmx_level >= 4) && (_sample_gap == 3)) \
      { \
        if (_buf_type == KDSC_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                SSSE3_INT16_FROM_UINT8_ILV3(_func); \
              } \
            else if (!_absolute) \
             { /* Convert floating point to bytes. */ \
               SSSE3_FLOATS_FROM_UINT8_ILV3(_func); \
             } \
          } \
      } \
    else if ((kdu_mmx_level >= 4) && (_sample_gap == 4)) \
      { \
        if (_buf_type == KDSC_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                SSSE3_INT16_FROM_UINT8_ILV4(_func); \
              } \
            else if (!_absolute) \
             { /* Convert floating point to bytes. */ \
               SSSE3_FLOATS_FROM_UINT8_ILV4(_func); \
             } \
          } \
      } \
  }


/* ========================================================================= */
/*              SIMD functions used by `kdu_stripe_decompressor'             */
/* ========================================================================= */

#  define SSSE3_INT16_TO_UINT8_RS_ILV1(_func)
#  define SSSE3_INT16_TO_UINT8_RS_ILV3(_func)
#  define SSSE3_INT16_TO_UINT8_RS_ILV4(_func)

#  define SSSE3_FLOATS_TO_UINT8_ILV1(_func)
#  define SSSE3_FLOATS_TO_UINT8_ILV3(_func)
#  define SSSE3_FLOATS_TO_UINT8_ILV4(_func)

#  define SSSE3_INT16_TO_INT16_ILV1(_func)
#  define SSSE3_INT32_TO_INT16_RS_ILV1(_func)
#  define SSSE3_FLOATS_TO_INT16_ILV1(_func)

#  define SSSE3_FLOATS_TO_FLOATS_ILV1(_func)

#ifndef KDU_NO_SSSE3
//----------------------------------------------------------------------------
extern void ssse3_int16_to_uint8_rs_ilv1(kdu_byte *, kdu_int16 **,
                                         int, int, int, bool, bool);
#undef SSSE3_INT16_TO_UINT8_RS_ILV1
#define SSSE3_INT16_TO_UINT8_RS_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_int16_to_uint8_rs_ilv1
//----------------------------------------------------------------------------
extern void ssse3_int16_to_uint8_rs_ilv3(kdu_byte *, kdu_int16 **,
                                         int, int, int, bool, bool);
#undef SSSE3_INT16_TO_UINT8_RS_ILV3
#define SSSE3_INT16_TO_UINT8_RS_ILV3(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_int16_to_uint8_rs_ilv3
//----------------------------------------------------------------------------
extern void ssse3_int16_to_uint8_rs_ilv4(kdu_byte *, kdu_int16 **,
                                         int, int, int, bool, bool);
#undef SSSE3_INT16_TO_UINT8_RS_ILV4
#define SSSE3_INT16_TO_UINT8_RS_ILV4(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_int16_to_uint8_rs_ilv4
//----------------------------------------------------------------------------
extern void ssse3_floats_to_uint8_ilv1(kdu_byte *, float **,
                                       int, int, int, bool, bool);
#undef SSSE3_FLOATS_TO_UINT8_ILV1
#define SSSE3_FLOATS_TO_UINT8_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_floats_to_uint8_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_to_uint8_ilv3(kdu_byte *, float **,
                                       int, int, int, bool, bool);
#undef SSSE3_FLOATS_TO_UINT8_ILV3
#define SSSE3_FLOATS_TO_UINT8_ILV3(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_floats_to_uint8_ilv3
//----------------------------------------------------------------------------
extern void ssse3_floats_to_uint8_ilv4(kdu_byte *, float **,
                                       int, int, int, bool, bool);
#undef SSSE3_FLOATS_TO_UINT8_ILV4
#define SSSE3_FLOATS_TO_UINT8_ILV4(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_floats_to_uint8_ilv4
//----------------------------------------------------------------------------
extern void ssse3_int16_to_int16_ilv1(kdu_int16 *, kdu_int16 **,
                                      int, int, int, bool, bool);
#undef SSSE3_INT16_TO_INT16_ILV1
#define SSSE3_INT16_TO_INT16_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_int16_to_int16_ilv1
//----------------------------------------------------------------------------
extern void ssse3_int32_to_int16_rs_ilv1(kdu_int16 *, kdu_int32 **,
                                         int, int, int, bool, bool);
#undef SSSE3_INT32_TO_INT16_RS_ILV1
#define SSSE3_INT32_TO_INT16_RS_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_int32_to_int16_rs_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_to_int16_ilv1(kdu_int16 *, float **,
                                       int, int, int, bool, bool);
#undef SSSE3_FLOATS_TO_INT16_ILV1
#define SSSE3_FLOATS_TO_INT16_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_floats_to_int16_ilv1
//----------------------------------------------------------------------------
extern void ssse3_floats_to_floats_ilv1(float *, float **,
                                        int, int, int, bool, bool);
#undef SSSE3_FLOATS_TO_FLOATS_ILV1
#define SSSE3_FLOATS_TO_FLOATS_ILV1(_func) \
  _func = (kdsd_simd_transfer_func) ssse3_floats_to_floats_ilv1
//----------------------------------------------------------------------------
#endif // !KDU_NO_SSSE3

#define KDSD_FIND_SIMD_TRANSFER_FUNC(_func, _buf_type, _shorts, \
                        _sample_gap, _prec, _orig_prec, _absolute) \
  /* NB: last 3 args are identical to those passed to the transfer func. */ \
  { \
    _func = NULL; /* Until proven otherwise. */ \
    if ((kdu_mmx_level >= 4) && (_sample_gap == 1)) \
      { \
        if (_buf_type == KDSD_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                if ((!_absolute) || ((_orig_prec<16) && (_orig_prec>=_prec))) \
                  { /* Conversion involves at most downshifts. */ \
                    SSSE3_INT16_TO_UINT8_RS_ILV1(_func); \
                  } \
              } \
            else if (!_absolute) \
              { /* Convert floating point to bytes. */ \
                SSSE3_FLOATS_TO_UINT8_ILV1(_func); \
              } \
          } \
        else if (_buf_type == KDSD_BUF16) \
          { \
            if (_shorts) \
              { /* Convert words to words, allowing left or right shifts. */ \
                if (_orig_prec < 16) \
                  SSSE3_INT16_TO_INT16_ILV1(_func); \
              } \
            else if (!_absolute) \
              { /* Convert floating point to words. */ \
                SSSE3_FLOATS_TO_INT16_ILV1(_func); \
              } \
            else if (_orig_prec >= _prec) \
              { /* Convert 32-bit ints to words, with possible rightshift. */ \
                SSSE3_INT32_TO_INT16_RS_ILV1(_func); \
              } \
          } \
        else if (_buf_type == KDSD_BUF_FLOAT) \
          { \
            if (!_absolute) \
              SSSE3_FLOATS_TO_FLOATS_ILV1(_func); \
          } \
      } \
    else if ((kdu_mmx_level >= 4) && (_sample_gap == 3)) \
      { \
        if (_buf_type == KDSD_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                if ((!_absolute) || ((_orig_prec<16) && (_orig_prec>=_prec))) \
                  { /* Conversion involves at most rightshifts. */ \
                    SSSE3_INT16_TO_UINT8_RS_ILV3(_func); \
                  } \
              } \
            else if (!_absolute) \
             { /* Convert floating point to bytes. */ \
               SSSE3_FLOATS_TO_UINT8_ILV3(_func); \
             } \
          } \
      } \
    else if ((kdu_mmx_level >= 4) && (_sample_gap == 4)) \
      { \
        if (_buf_type == KDSD_BUF8) \
          { \
            if (_shorts) \
              { /* Convert fixed point or reversible ints to bytes. */ \
                if ((!_absolute) || ((_orig_prec<16) && (_orig_prec>=_prec))) \
                  { /* Conversion involves at most rightshifts. */ \
                    SSSE3_INT16_TO_UINT8_RS_ILV4(_func); \
                  } \
              } \
            else if (!_absolute) \
             { /* Convert floating point to bytes. */ \
               SSSE3_FLOATS_TO_UINT8_ILV4(_func); \
             } \
          } \
      } \
  }

#endif // X86_STRIPE_TRANSFER_LOCAL_H
