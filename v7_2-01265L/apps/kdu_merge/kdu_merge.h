/*****************************************************************************/
// File: kdu_merge.h [scope = APPS/MERGE]
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
   Header file for "kdu_merge.cpp"
******************************************************************************/
#ifndef KDU_MERGE_H
#define KDU_MERGE_H

#include "jpx.h"
#include "mj2.h"

// Defined here:
struct mg_source_spec;
struct mg_channel_spec;
struct mg_layer_spec;
struct mg_codestream_spec;
struct mg_container_spec;
struct mg_track_seg;
struct mg_track_spec;

/*****************************************************************************/
/*                               mg_source_spec                              */
/*****************************************************************************/

struct mg_source_spec {
  public: // Member functions
    mg_source_spec()
      { 
        filename = NULL;
        metadata_source = this; // Import metadata from ourself by default
        num_codestreams = num_layers = num_frames = num_fields = 0;
        field_order = KDU_FIELDS_NONE;
        first_frame_idx = 0; codestream_specs = NULL;
        next=NULL; video_source = NULL;
      }
    ~mg_source_spec()
      { 
        if (filename != NULL)
          delete[] filename;
        if (codestream_specs != NULL)
          delete[] codestream_specs;
        jpx_src.close();
        mj2_src.close();
        family_src.close();
      }
  public: // Data
    char *filename;
    kdu_simple_file_source raw_src; // Only opened on demand
    kdu_coords raw_codestream_size; // As would be returned by `jp2_dimensions'
    jp2_family_src family_src; // We leave this open for convenience
    jpx_source jpx_src; // We leave this open for convenience
    mj2_source mj2_src; // We leave this open for convenience
    mj2_video_source *video_source; // For MJ2 tracks
    mg_source_spec *metadata_source; // Object from which to import metadata
    int num_codestreams;
    mg_codestream_spec **codestream_specs; // See below
    int num_layers; // One layer per codestream for MJ2 & raw sources
    int num_frames; // One frame per layer for JPX sources
    int num_fields; // Number of video fields which can be made from the source
    kdu_field_order field_order; // For MJ2 sources
    int first_frame_idx; // Seeking offset for first used frame in MJ2 track
    mg_source_spec *next;
  };
  /* Notes:
        The `codestream_specs' array has `num_codestreams' entries that are
     initialized to NULL.  If a codestream in the source file is found to
     be required for the output file, its entry in this array is initialized
     to point to the relevant `mg_codestream_spec' object. */

/*****************************************************************************/
/*                             mg_codestream_spec                            */
/*****************************************************************************/

struct mg_codestream_spec {
  mg_codestream_spec()
    { this->source=NULL; this->next=NULL; }
  public: // Data
    int out_codestream_idx;
    mg_source_spec *source;
    int source_codestream_idx;
    jpx_codestream_target tgt;
    mg_codestream_spec *next;
  };

/*****************************************************************************/
/*                               mg_channel_spec                             */
/*****************************************************************************/

struct mg_channel_spec { // For JPX output files
  public: // Member functions
    mg_channel_spec()
      { file=NULL; codestream_idx=component_idx=lut_idx=-1; next=NULL; }
  public: // Data
    mg_source_spec *file;
    int codestream_idx; // Index of codestream within `file'
    int component_idx;
    int lut_idx;
    mg_channel_spec *next;
  };

/*****************************************************************************/
/*                               mg_layer_spec                               */
/*****************************************************************************/

struct mg_layer_spec { // For JPX output files
  public: // Member functions
    mg_layer_spec(int idx)
      { file=NULL; channels=NULL; used_codestreams=NULL; next=NULL;
        album_page_idx=-1; source_layer_idx=0; out_layer_idx=idx;
        num_used_codestreams=0; num_colour_channels=0; num_alpha_channels=0; }
    ~mg_layer_spec()
      { mg_channel_spec *cp;
        while ((cp=channels) != NULL)
          { channels=cp->next; delete cp; }
        if (used_codestreams != NULL)
          delete[] used_codestreams;
      }
  public: // Members for existing layes
    mg_source_spec *file; // NULL if we are building a layer from scratch
    int source_layer_idx;
  public: // Members for building a layer from scratch
    jp2_colour_space space;
    int num_colour_channels;
    int num_alpha_channels; // At most 1
    mg_channel_spec *channels; // Linked list
  public: // Members used to keep track of codestreams used by this layer
    int num_used_codestreams;
    int *used_codestreams; // Set of output codestream indices used by layer
  public: // Common members
    int out_layer_idx;
    mg_layer_spec *next;
    kdu_coords size; // Size of the first code-stream used by the layer; filled
                     // in when writing layers to output file.
    int album_page_idx; // Index of of the frame which contains the album index
       // page on which this layer appears -- -ve if not part of an album.
  };

/*****************************************************************************/
/*                            mg_container_spec                              */
/*****************************************************************************/

struct mg_container_spec {
  public: // Member functions
    mg_container_spec()
      { base_layers=NULL; num_tracks = 0; next=NULL; }
    ~mg_container_spec()
      { if (base_layers != NULL) delete[] base_layers; }
  public: // Data
    int num_repetitions; // Can be 0 if indefinite
    int num_base_layers;
    mg_layer_spec **base_layers;
    int num_base_codestreams;
    int first_base_codestream_idx;
    int num_tracks; // Number of tracks added so far
    jpx_container_target tgt;
    mg_container_spec *next;
  };

/*****************************************************************************/
/*                               mg_track_seg                                */
/*****************************************************************************/

struct mg_track_seg { // For MJ2 output files
  public: // Member functions
    mg_track_seg()
      { from=to=0;  fps=0.0F;  next = NULL; }
  public: // Data
    int from, to;
    float fps;
    mg_track_seg *next;
  };

/*****************************************************************************/
/*                               mg_track_spec                               */
/*****************************************************************************/

struct mg_track_spec { // For MJ2 output files
  public: // Member functions
    mg_track_spec()
      { field_order = KDU_FIELDS_NONE;  segs = NULL;  next = NULL; }
    ~mg_track_spec()
      { mg_track_seg *tmp;
        while ((tmp=segs) != NULL)
          { segs = tmp->next; delete tmp; }
      }
  public: // Data
    kdu_field_order field_order;
    mg_track_seg *segs;
    mg_track_spec *next;
  };

#endif // KDU_MERGE_H
