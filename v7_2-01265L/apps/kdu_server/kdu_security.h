/*****************************************************************************/
// File: kdu_security.h [scope = APPS/CLIENT-SERVER]
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
   Header file for `kdu_security.cpp', advertising some elementary
encryption services.
******************************************************************************/

#ifndef KDU_SECURITY_H
#define KDU_SECURITY_H

#include <assert.h>
#include <string.h>
#include "kdu_elementary.h"
#include "kdu_messaging.h"

// Defined here:
class kdu_rijndael;

/*****************************************************************************/
/*                              kdu_rijndael                                 */
/*****************************************************************************/

#define KD_NUM_ROUNDS 10

class kdu_rijndael {
  public: // Member functions
    kdu_rijndael()
      { for (int i=0; i < 16; i++) key[i]=0; key_set = false; }
    void set_key(const char *string);
      /* Convert a text string of any length into a suitable 128-bit key. */
    void set_key(kdu_byte key[]);
      /* Initialize the 128-bit key using the supplied 16-byte array. */
    bool ready() { return key_set; }
    bool operator!() { return !key_set; }
    void encrypt(kdu_byte data[]);
      /* Encrypt the supplied 16-byte array of data using the existing key.
         You must call one of the `set_key' functions before using this
         function.  All processing is performed in place, converting the
         `data' array into its encrypted code bytes. */
    void decrypt(kdu_byte data[]);
      /* Same as `encrypt' but reverses the process.  All processing is
         performed in place, as with `encrypt'. */
  private: // Helper functions
    void shift_rows_left(kdu_byte data[]);
      /* Cyclically shifts row k to the left by k where k=0,1,2,3.  Data is
         organized by columns, with 4 columns, each of height 4. */
    void mix_columns(kdu_byte data[]);
  private: // data
    kdu_byte key[16];
    kdu_byte round_key[16*(KD_NUM_ROUNDS+1)];
    bool key_set;
  };

#endif // KDU_SECURITY_H
