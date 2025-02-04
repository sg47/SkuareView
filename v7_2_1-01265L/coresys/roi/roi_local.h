/*****************************************************************************/
// File: roi_local.h [scope = CORESYS/ROI]
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
   Local definitions used by "roi.cpp".  These are not to be included
from any other scope.
******************************************************************************/
#ifndef ROI_LOCAL_H
#define ROI_LOCAL_H

#include "kdu_compressed.h"
#include "kdu_roi_processing.h"

// Defined here:
class kd_roi_level_node;
class kd_roi_level;

/*****************************************************************************/
/*                             kd_roi_level_node                             */
/*****************************************************************************/

class kd_roi_level_node : public kdu_roi_node {
  public: // Member functions
    kd_roi_level_node(kd_roi_level *owner, kdu_coords size)
      {
        this->owner = owner; available = true; active = false;
        cols = size.x; remaining_rows = size.y;
        num_row_buffers = first_valid_row_buffer = num_valid_row_buffers = 0;
        row_buffers = NULL;
      }
    virtual ~kd_roi_level_node(); // No need to be virtual, but keeps gcc happy
    void pull(kdu_byte buf[], int width);
    kdu_byte *advance();
      /* Augments the internal buffer of valid rows, returning to the caller
         a pointer to the new buffer whose contents are to be filled in.
         The function returns NULL if the object is inactive, meaning that
         nobody has acquired the node.  Once such a condition has occurred,
         nobody will be allowed to acquire the node. */
    void acquire()
      /* Called when somebody acquires the node using the containing
         `kdu_roi_level' object's `acquire_node' function. */
      { assert(available); available = false; active = true; }
    void release();
      // Overrides base member.
  private: // Data
    kd_roi_level *owner;
    bool available; // True until the first `acquire' or `advance' call.
    bool active; // True after `acquire' and prior to `release'.
    int cols, remaining_rows;
    int num_row_buffers;
    int first_valid_row_buffer;
    int num_valid_row_buffers;
    kdu_byte **row_buffers;
  };
  /* Notes:
        This object manages roi information for a single subband produced by
     the containing `kd_roi_level' object.  The dimensions of the subband
     are supplied as the constructor's `size' argument, while the containing
     `kd_roi_level' object is supplied as the constructor's `owner' argument.
        The object essentially manages a dynamically sized buffer for rows
     of ROI information which have been generated by the owner and pushed
     into the node using its `advance' member function, but have not yet
     been requested by the client through the base class's `pull' interface
     function.  When a request arrives for a new line and none are available,
     the owner's `advance' member function is called until the required data
     have become available.
        The `row_buffers' array points to `num_row_buffers' byte arrays, each
     with `cols' entries.  The buffer may be dynamically resized as
     necessary.  Of these `row_buffers' arrays, only `num_valid_row_buffers'
     actually contain valid data pushed into the object via its `push'
     member function.  The first of these is located at the position in
     `row_buffers' identified by `first_valid_row_buffer'.  The `row_buffers'
     array is understood as implementing a circular buffer, so that the
     first element in the array immediately follows the last element. */

/*****************************************************************************/
/*                                kd_roi_level                               */
/*****************************************************************************/

class kd_roi_level {
  public: // Member functions
    kd_roi_level()
      { source = NULL; nodes[0]=nodes[1]=nodes[2]=nodes[3]=NULL;
        num_row_buffers = 0; row_buffers = NULL; out_buf = NULL; }
    ~kd_roi_level();
    void init(kdu_node node, kdu_roi_node *source);
    void advance();
      /* This function does all the work.  It is called by any of the
         subband nodes when it receives a `pull' request for which the
         data has not already been generated. */
    void notify_release(kd_roi_level_node *caller);
      /* Called by one of the four descendant nodes when its own `release'
         function is called, to notify the current object that there is no
         need to continue servicing this node. */
  private: // Data
    friend class kdu_roi_level;
    kdu_roi_node *source;
    kd_roi_level_node *nodes[4]; // Up to four children
    bool node_released[4]; // When all elements are true, can release `source'
    int num_nodes_released; // Count of true elements in `node_released'
    kdu_dims dims; // Location and dimensions on canvas.
    int next_row_loc; // Canvas location of next row to generate
    int first_valid_row_loc; // Canvas location of first row in the buffer
    int num_valid_rows; // Number of rows in the buffer
    kdu_coords support_min[2], support_max[2]; // See below
    bool split_vertically, split_horizontally;

      // The following members manage the circular row buffer
    int num_row_buffers;
    int first_buffer_idx;
    kdu_byte **row_buffers;
    kdu_byte *out_buf; // Used for storing the output of vertical processing.
  };
  /* Notes:
        The object manages storage for sufficient rows of ROI mask data
     to enable the computation of ROI mask data for a single row from each
     of the vertically low- and high-pass subbands.  The buffered rows are
     kept in the `row_buffers' array which implements a circular buffer.  The
     first row managed by the circular buffer is at array index
     `first_buffer_idx', while the total size of the circular buffer is
     given by `num_row_buffers'.
        The `next_row_loc' field holds the vertical canvas coordinate of the
     next row to be produced by the `advance' function.  If even, the next
     row is a low-pass vertical subband row; otherwise, it is a high-pass
     subband row.  Lower and upper bounds for the support of the low- and
     high-pass synthesis kernels are managed by `support_min[0]' and
     `support_min[1]' respectively.  These are each 2-D vectors, whose `x'
     member holds the support information for the horizontal kernel and
     whose `y' member holds support information for the vertical kernel.  It
     is possible that the horizontal and vertical synthesis filters will
     be different if the `kdu_codestream' machinery is operating in a mode
     where one of the horizontal and vertical directions is flipped, while
     the other is not, and the underlying transform kernel is not whole-sample
     symmetric.  Remember that the definition of the synthesis filter impulse
     responses is based on the convention that high-pass synthesis filters are
     excited by samples at odd sample locations, while low-pass synthesis
     filters are excited by samples at even locations, as explained in the
     description of `kdu_kernels::get_impulse_response'.
        The `first_valid_row_loc' field holds the vertical canvas
     coordinate of the first row stored in the buffer, while
     `num_valid_rows' identifies the number of rows actually stored in
     the buffer at the current time.  These may be used to identify the
     range of rows in the buffer which are required for the computation
     of ROI mask information in the vertical subbands, and also to determine
     whether or not new rows need to be recovered from the ROI source
     object supplied during construction. */

#endif // ROI_LOCAL_H
