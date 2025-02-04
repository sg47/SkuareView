/*****************************************************************************/
// File: kdu_servex.h [scope = APPS/SERVER]
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
   Header file for the `kdu_servex' object which provides elementary
support for preparing raw code-streams and JP2/JPX files to be served by the
"kdu_server" application.  The implementation of `jpx_serve_target' is
designed to be platform independent, pushing all platform dependencies
into the "kdu_server" application itself, rather than its re-usable
components.
******************************************************************************/

#ifndef KDU_SERVEX_H
#define KDU_SERVEX_H

#include <stdio.h>
#include "kdu_serve.h"
#include "jp2_shared.h"

// Defined here:
class kdu_servex;

// Defined elsewhere:
struct kdsx_open_file;
struct kdsx_entity_container;
class kdsx_image_entities;
struct kdsx_metagroup;
struct kdsx_stream_suminfo;
class kdsx_stream;
class kdsx_context_mappings;

/*****************************************************************************/
/*                                kdu_servex                                 */
/*****************************************************************************/

class kdu_servex : public kdu_serve_target {
  /* [BIND: reference]
     [SYNOPSIS]
       Implements the `kdu_serve_target' interface for raw code-streams, JP2
       and JPX files.
  */
  public: // Member functions
    KDU_AUX_EXPORT kdu_servex();
    ~kdu_servex()
      { 
        close();
        codestream_mutex.destroy();
        primary_mutex.destroy();
        if (scratch_buf != NULL) delete[] scratch_buf;
      }
    KDU_AUX_EXPORT void
      open(const char *filename, int phld_threshold, int per_client_cache,
           FILE *cache_fp, bool cache_exists,
           kdu_long sub_start=0, kdu_long sub_lim=KDU_LONG_MAX);
      /* [SYNOPSIS]
           Opens the target file, which must either be a JP2-compatible file
           or a raw JPEG2000 code-stream.  You may, optionally qualify the
           target file with a byte-range (a sub-range).  In this case, the
           raw code-stream or set of JP2-family boxes which make up the
           target are found at the `sub_start' position and
           `sub_lim'-`sub_start' is the maximum length of the target byte
           range.  Note, however, that the supplied byte range will be ignored
           if the structure of the target has already been cached in an
           existing cache file (see `cache_fp' and `cache_exists').
         [ARG: phld_threshold]
           This argument is used to control the way in which meta-data is
           partitioned into meta data-bins.  Specifically, any JP2 box whose
           total size exceeds the supplied `phld_threshold' is automatically
           replaced by a placeholder (`phld') box in the streaming
           representation.  The placeholder box references another data-bin
           which holds the contents of the box.  By selecting a small
           threshold, the meta-data may be split across numerous meta data-bins
           each of which will be delivered to the client only if it is
           relevant to the client's request.  Note, however, that this argument
           has no effect if the `cache_exists' argument is true, since then
           the partitioning of meta-data to data-bins has already taken place
           and the results have been stored in the `cache_fp' file.   It is
           important that the server use whatever partition was previously
           created so that the client will encounter a consistent
           representation each time it receives information from the
           file.
         [ARG: per_client_cache]
           This argument is used to control the size of the cache internally
           managed by each `kdu_codestream' object created by this function.
           Each time a `kdu_serve' object invokes `attach_to_codestream',
           the code-stream's maximum cache size is augmented by this amount
           (see `kdu_codestream::augment_cache_threshold').  Each time the
           `detach_from_codestream' function is invoked, the cache threshold
           is reduced by this amount.
         [ARG: cache_fp]
           If NULL, there will be no caching of the meta-data or stream
           identification structure created by this object, so no unique target
           identifier will be issued to the client.  Otherwise, this
           argument points to a file which is open either for reading or
           for writing.  If `cache_exists' is true, the file is open for
           reading and the meta-data and stream identification structure
           should be recovered by parsing the file's contents.  Otherwise,
           the file is open for writing; after generating the meta-data
           and stream identification structure, the object should save this
           structure to the supplied file for later re-use.  This ensures
           that the file will be presented in exactly the same way every
           time.  Cache files are associated by the "kdu_server" application
           with a unique target identifier.  Note that the present function
           will not close an open cache file.
        [ARG: cache_exists]
           True if a non-NULL `cache_fp' argument refers to a file which is
           open for reading.
      */
    KDU_AUX_EXPORT void close();
      /* [SYNOPSIS]
           It is safe to call this function at any time, whether the object
           is open or not, but of course there should be no `kdu_serve' objects
           to which it is attached when you close it.
      */
    virtual int *get_codestream_ranges(int &num_ranges,
                                       int compositing_layer_idx)
      {
        if (compositing_layer_idx >= 0)
          { num_ranges = 0; return NULL; }
        else
          { num_ranges = 1; return codestream_range; }
      }
      /* [SYNOPSIS]
           Implements `kdu_serve_target::get_codestream_ranges'.
      */
    KDU_AUX_EXPORT virtual bool
      get_codestream_siz_info(int codestream_id, kdu_dims &image_dims,
                              kdu_dims &tile_partition, kdu_dims &tile_indices,
                              int &num_components, int &num_output_components,
                              int &max_discard_levels, int &max_quality_layers,
                              kdu_coords *component_subs=NULL,
                              kdu_coords *output_component_subs=NULL);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::get_codestream_siz_info'.
      */
    KDU_AUX_EXPORT virtual bool
      get_codestream_rd_info(int codestream_id,
                             int &num_layer_slopes, int &num_layer_lengths,
                             int *layer_log_slopes=NULL,
                             kdu_long *layer_lengths=NULL);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::get_codestream_rd_info'.
      */  
    KDU_AUX_EXPORT virtual
      kdu_codestream attach_to_codestream(int codestream_id,
                                          kd_serve *thread_handle);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::attach_to_codestream'.
      */
    KDU_AUX_EXPORT virtual void
      detach_from_codestream(int codestream_id, kd_serve *thread_handle);
      /* [SYNOPSIS]
            Implements `kdu_serve_target::detach_from_codestream'.
      */
    KDU_AUX_EXPORT virtual void
      lock_codestreams(int num_codestreams, int *codestream_indices,
                       kd_serve *thread_handle);
      /* [SYNOPSIS]
            Implements `kdu_serve_target::lock_codestream'.
      */
    KDU_AUX_EXPORT virtual void
      release_codestreams(int num_codestreams, int *codestream_indices,
                          kd_serve *thread_handle);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::release_codestream'.
      */
    virtual kdu_window_context
      access_context(int context_type, int context_idx, int remapping_ids[]);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::access_context'.
      */
    virtual const kds_metagroup *get_metatree()
      { return (const kds_metagroup *) metatree; }
      /* [SYNOPSIS]
           Implements `kdu_serve_target::get_metatree'.
      */
    KDU_AUX_EXPORT virtual int
      read_metagroup(const kds_metagroup *group, kdu_byte *buf,
                     int offset, int num_bytes);
      /* [SYNOPSIS]
           Implements `kdu_serve_target::read_metagroup'.
      */
  private: // Helper functions
    friend struct kdsx_open_file;
    friend class kdsx_image_entities;
    friend class kdsx_stream;
    friend struct kdsx_stream_suminfo;
    friend struct kdsx_metagroup;
    void create_structure(kdu_long sub_start, kdu_long sub_lim,
                          int phld_threshold);
      /* Called from `open' if no cached version of the metadata
         and codestream structure exists. */
    void enable_codestream_access(int per_client_cache);
      /* Installs the `per_client_cache' value and the file names for each
         code-stream so that they can be opened. */
    void read_codestream_summary_info();
      /* Called after `create_structure' and `enable_codestream_access' to read
         summary information for each codestream.  This information will be
         saved by `save_structure' so that the main server can know some
         information about codestreams before it opens them -- this information
         is used to determine ahead of time how and when the codestreams will
         be opened. */
    void save_structure(FILE *cache_file);
      /* Saves the structure to the indicated cache file. */
    void load_structure(FILE *cache_file);
      /* Loads the structure from the indicated cache file. */
    kdsx_entity_container *add_container(int num_jpch, int num_jplh);
      /* This function is invoked when a Compositing Layer Extensions box is
         parsed; the function adds a new container to its internal list and
         invokes its `init' function with appropriate parameters.  To this
         end, the `num_jpch' and `num_jplh' argument should represent the
         total number of codestream header boxes and compositing layer header
         boxes encountered so far.  These values are used only to
         initialize the first JPX container; subsequent ones are initialized
         based on the properties of the most recently completed container. */
    kdsx_stream *add_stream(int stream_id);
      /* Creates a new `kdsx_stream' object, adding it to the list of all
         codestreams and entering a reference to it in the `stream_refs'
         array.  This may cause the `stream_refs' array to be expanded and
         some NULL entries may also need to be entered to account for
         codestreams which are missing because a streaming equivalent could
         not be created (e.g., where fragment tables have been used). */
    kdsx_image_entities *get_temp_entities();
      /* Used when building scope for `kdsx_metagroup'. */
    kdsx_image_entities *
      commit_image_entities(kdsx_image_entities *tmp,
                            kdsx_entity_container *container);
      /* Used when scope is complete for a `kdsx_metagroup', this function
         tries to reuse existing image entity lists.  Returns a pointer to
         a new or existing resource which has the same content as the `tmp'
         object, recycling `tmp' to the temporary image entities list.
         If `container is non-NULL, the function uses the `container'
         object's committed entity list; otherwise, the `tmp->container'
         member must be NULL.  This function returns objects whose `ref_id'
         is non-negative, distinguishing them from temporary objects
         returned by `get_temp_entities'. */
    kdsx_image_entities *
      get_parsed_image_entities(kdu_int32 container_ref_id, kdu_int32 ref_id);
      /* Returns the element of a cached image entities list whose index
         is given by `ref_id'.  The relevant list is determined by the
         `container_ref_id' argument -- if 0, the top-level entities list
         is used; otherwise, the entities list belongs to the entity
         container with the corresponding `ref_id' -- the index of this
         entity container within the `container_refs' array should be
         `container_ref_id'-1. */
    kdsx_open_file *get_open_file(const char *fname);
      /* This function returns a `kdsx_open_file' object whose `add_user'
         function will generally be invoked by the caller -- but it need not
         be.  The function scans the `locked_files', `used_files' and
         `unused_files' lists to see if an open file already exists with the
         indicated file name.  If so, the function simply returns a pointer to
         that object -- it does not add any users and locks.  If no suitable
         file can be found, the function creates a new `kdsx_open_file' object
         and places it on the `unused_files' list for the moment.  However,
         before doing this, the function attempts to close and remove one
         entry from the `unused_files' list, if there is one.  If there is
         no unused file, but there is an unlocked file, and the total number
         of open files exceeds some threshold, the function may attempt to
         remove all users from the least recently used unlocked file and then
         close it before opening the new file.
            This file is only called with `codestream_mutex' locked, except
         in when creating a cache representation during the call to `open'. */
    kdu_byte *get_scratch_buf(size_t len)
      { /* Used to read/write arrays in large chunks. */
        if (len > scratch_buf_len)
          { 
            if (scratch_buf != NULL) delete[] scratch_buf;
            scratch_buf = NULL;
            scratch_buf = new kdu_byte[len];
            scratch_buf_len = len;
          }
        return scratch_buf;
      }
  private: // Data
    kdu_mutex primary_mutex;    // See the explanation of these two different
    kdu_mutex codestream_mutex; // mutex locks in the notes below
    kd_serve *locking_thread_handle; // Uniquely identifies thread that has
    int num_locked_streams;          // locked one or more streams for access.
    kdsx_open_file *locked_files; // Files used by streams locked for access
    kdsx_open_file *used_files_head; // Files not locked but with at least one
    kdsx_open_file *used_files_tail; // user; head is considered the LRU elt.
    kdsx_open_file *unused_files_head; // Files that are open but not used;
    kdsx_open_file *unused_files_tail; // hhead is considered the LRU element.
    int num_open_files; // Number of files on above lists
    char *target_filename; // Copy of file name passed into `open'
    int codestream_range[2]; // `from' and `to' (inclusive) indices
    kdsx_metagroup *metatree;
    FILE *meta_fp; // Open file pointer for use in meta-data accesses.
    kdsx_stream *stream_head; // List of available code-streams
    kdsx_stream *stream_tail; // Last element in above list
    kdsx_stream **stream_refs; // Array of pointers to codestreams
    int max_stream_refs; // Size of above array
    kdsx_entity_container *container_head;
    kdsx_entity_container *container_tail;
    kdsx_entity_container **container_refs;
    int num_containers;
  private: // Default codestream summary data
    kdsx_stream_suminfo *default_stream_suminfo;
  private: // Top-level image entitites and context mappings
    kdsx_image_entities *tmp_entities; // Active temp entity lists
    kdsx_image_entities *free_tmp_entities; // List of free temp list
    kdsx_image_entities *committed_entities_list;
    int num_committed_entities; // Becomes number of elements in below array
    kdsx_image_entities **committed_entity_refs; // Ptrs into committed list
    kdsx_context_mappings *top_context_mappings;
    j2_data_references data_references;  
  private: // Temporary resources
    size_t scratch_buf_len;
    kdu_byte *scratch_buf;
  };
  /* Notes:
       The object provides exactly two locking objects, `primary_mutex' and
     `codestream_mutex', so as to avoid excessive consumption of file handles.
     Correct use and understanding of these mutexes is required to avoid
     deadlock.  The intended usage pattern is as follows:
     1) `primary_mutex' protects use of the `meta_fp' file pointer.
     2) `codestream_mutex' protects access to the `open_file_head/tail' list
        and all attach/detach/lock/release operations on entries in the
        `stream_head/tail' list.
     3) `locking_thread_handle' is used in connection with `codestream_mutex'
        to keep track of any thread that has locked `codestream_mutex'.  In
        particular, the `attach_to_codestream' and `detach_from_codestream'
        functions need to temporarily lock the `codestream_mutex', unless
        they find that the calling thread already has the required lock.
  */

#endif // KDU_SERVEX_H
