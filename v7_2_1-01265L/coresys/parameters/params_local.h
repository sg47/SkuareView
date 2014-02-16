/*****************************************************************************/
// File: params_local.h [scope = CORESYS/PARAMETERS]
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
   Local definitions for use by "params.cpp".  These should not be included
from any other scope.
******************************************************************************/

#ifndef PARAMS_LOCAL_H
#define PARAMS_LOCAL_H

// Defined here:

struct att_val;
struct kd_attribute;

/*****************************************************************************/
/*                                    att_val                                */
/*****************************************************************************/

struct att_val {
  /* Stores a single attribute value.  If `pattern' points to a string
     whose first character is 'F', the value is a floating point quantity.
     Otherwise, the value is an integer. */
    att_val()
      { is_set = false; pattern=NULL; }
    union {
      int ival;
      float fval;
      };
    char const *pattern;
    bool is_set;
  };

/*****************************************************************************/
/*                                  kd_attribute                             */
/*****************************************************************************/

struct kd_attribute {
  /* Objects of this class are used to build a linked list of attributes,
     which are managed by the kdu_params class. An attribute may contain
     one or more parameter records, each of which may contain one or more
     fields. Each field may have a different data type and  interpretation. */
  public: // Member functions
    kd_attribute(const char *name, const char *comment,
                 int flags, const char *pattern);
      /* See the definition of `kdu_params::define_attribute'. */
    ~kd_attribute()
      { delete[](values); }
    void augment_records(int new_records);
    void describe(kdu_message &output, bool allow_tiles, bool allow_comps,
                  bool treat_instances_like_components, bool include_comments);
    bool remove_unmarked_records()
      { // Invoked from `kdu_params::finalize_all' to prevent accidental
        // use of content from a previous codestream when a new codestream's
        // parameters are translated after a call to `kdu_codestream::restart'
        assert(num_used_records >= num_marked_records);
        if (num_used_records == num_marked_records)
          return false;
        num_used_records = num_marked_records;
        return true;
      }
  public: // Data
    const char *name; // See constructor.
    const char *comment; // See constructor.
    int flags; // See constructor.
    const char *pattern; // See constructor.
    int num_fields; // Number of fields in each record (i.e., in `pattern').
    int num_used_records; // Number of records which have ever been written.
    int num_marked_records; // See below
    att_val *values; // Array of `max_records'*`num_fields' values.
    bool derived; // Set using the `kdu_params::set_derived' function.
    bool parsed; // Set if the information was obtained by string parsing.
    kd_attribute *next; // Used to build linked list within kdu_params class
  private:
    int max_records; // Maximum storage available in `values' array.
  };
  /* Notes:
        The `num_marked_records' member is a state variable that keeps track
     of the number of records that span all calls to `kdu_params::set' that
     have set values for this attribute.  This value is usually identical to
     `num_used_records', except that `kdu_params::clear_marks' resets the
     value to 0 so that we can tell the difference between values that were
     set before `clear_marks' and values set afterwards when re-using the
     parameter system for a new codestream -- important to
     `kdu_codestream::restart'.  All access functions (e.g., `kdu_params::get')
     interpret `num_marked_records' as the number of available records that
     can be legitimately read.  The `set' functions perform comparisons
     between new values and old values within the first `num_used_records'
     to determine whether or not there are any changes. */

#endif // PARAMS_LOCAL_H
