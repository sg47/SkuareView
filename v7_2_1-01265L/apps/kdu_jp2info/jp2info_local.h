/*****************************************************************************/
// File: jp2info_local.h [scope = APPS/JP2INFO]
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
   Local definitions used by "kdu_jp2info.cpp".
******************************************************************************/

#ifndef JP2INFO_LOCAL_H
#define JP2INFO_LOCAL_H

#include <stdio.h>
#include <assert.h>
#include "kdu_messaging.h"

/*****************************************************************************/
/*                            kd_indented_message                            */
/*****************************************************************************/

class kd_indented_message : public kdu_message {
  public: // Member functions
    kd_indented_message()
      { indent = 0; at_start_of_line = true; }
    virtual ~kd_indented_message() { return; }
    void set_indent(int num_spaces)
      { indent = num_spaces; }
    virtual void put_text(const char *string)
      { 
        for (; *string != '\0'; string++)
          { 
            if (at_start_of_line)
              issue_prefix();
            putc(*string,stdout);
            if (*string == '\n')
              at_start_of_line = true;
          }
      }
      /* Overrides `kdu_message::put_text'.  Passes text through to stdout,
         except that each line is indented by the amount specified by the
         most recent call to `set_indent'. */
    virtual void flush(bool end_of_message)
      { 
        if (end_of_message && !at_start_of_line)
          { putc('\n',stdout); at_start_of_line = true; }
      }
      /* Overrides `kdu_message::flush'.  Does nothing unless `end_of_message'
         is true, in which case the function terminates any currently
         unterminated line. */
  private: // Helper functions
    void issue_prefix()
      { 
        assert(at_start_of_line);
        for (int s=indent; s > 0; s--)
          putc(' ',stdout);
        at_start_of_line = false;
      }
  private: // Data
    int indent;
    bool at_start_of_line;
  };
  /* Notes:
       This object just passes all text through to stdout, except that
     each line is prefixed by an indentation string, as set by calls to
     `set_indent'. */

#endif // JP2INFO_LOCAL_H
