/*****************************************************************************/
//  
//  @file: hdf5_local.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Sean Peters
//  @date 18/02/2013
//  @brief The The file contains the definitions of types and classes
//  @brief for the HDF5 image format.
//
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
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
/******************************************************************************/

// Specific to ICRAR's hdf5 image format.
#define DATASET_NAME "full_cube"
#define H5_FLOAT_MIN -.006383
#define H5_FLOAT_MAX 0.105909
//#define H5_FLOAT_MIN -0.000354053
//#define H5_FLOAT_MAX 0.00106955

#include <stdio.h> // C I/O functions can be quite a bit faster than C++ ones
#include <fstream>
#include "kdu_elementary.h"
#include "kdu_file_io.h"
#include "hdf5.h"
#include "ska_local.h"

/**
 * Structure for defining essential properties of a HDF5 datacube.
 */
typedef struct {
    long width; // Image width
    long height; // Image height
    long depth; // Image depth. Arbitrary for 2D images
    int naxis;
    int t_class; // Image data type.  Similar to BITPIX in CFITSIO
} hdf5_cube_info;

class hdf5_in;
class hdf5_out;

/*****************************************************************************/
/*                             class hdf5_in                                 */
/*****************************************************************************/

class hdf5_in : public ska_source_file_base {
  public: // Member functions
    ~hdf5_in();
    void read_header(jp2_family_tgt &tgt, kdu_args &args, 
        ska_source_file * const source_file);
    void read_stripe(int height, kdu_byte *buf, 
        ska_source_file * const source_file);
  private: // Members describing the organization of the HDF5 data
    hid_t file; // File handle for the HDF5 handle
    hid_t dataset;
    hid_t dataspace;
    hid_t datatype;
    hid_t memspace;
    H5T_order_t order; // Data order (littlendian or bigendian)
    hsize_t* dims_mem; // Dimensions of data stored in memory
    hsize_t* offset; // The offset of the dimensions of the HDF5 image that we're 
                     // converting.
    hsize_t* offset_out; // Offset within the already selected hyperslab
    // The extent of each of the dimensions of the hyperslab in the file. i.e. 
    // length, breadth, etc.
    hsize_t* extent; 
    // Describes the entire HDF5 cube and not just the selection we are encoding
    hdf5_cube_info cinfo;       
    short domain;
   
    bool littlendian; // true if data order is littlendian
    int first_comp_idx;
    int num_unread_rows; // Always starts at `rows', even with cropping
    int total_rows; // Used for progress bar
  private: // Members which are affected by (or support) cropping
    bool parse_hdf5_parameters(jp2_family_tgt &tgt, kdu_args &args);
};

/*****************************************************************************/
/*                               class hdf5_out                              */
/*****************************************************************************/

class hdf5_out : public ska_dest_file_base {
  public: // Member functions
    ~hdf5_out();
    void write_header(jp2_family_tgt &tgt, kdu_args &args,
        ska_dest_file* const dest_file);      
    void write_stripe(int height, kdu_byte *buf,
        ska_dest_file* const dest_file);
  private: // Data
    // Describes the HDF5 cube we are writing to
    hdf5_cube_info cinfo; 
    hid_t file; // File handle for the HDF5 file
    hid_t dataset;
    hid_t dataspace;
    hid_t datatype;
    hid_t filespace;
    hid_t memspace;
    hid_t cparms;
    H5T_order_t order;

    hsize_t* orig_dims; // Dimensions of origional JPEG2000 image
    hsize_t* dims_mem; // The hyperslab dimension we select for each iteration
                       // of hdf5_out::put
    hsize_t* dest_dims; // Dimensions of destination HDF5 image
    hsize_t* offset; // Offset within the JPEG2000 image

    int num_components;
    short domain;

    int scanline_width, unpacked_samples;
    int sample_bytes, pixel_bytes, row_bytes;
    bool pre_pack_littlendian; // Scanline byte order prior to packing
    int num_unwritten_rows;
    //--------------------------------------------------------------------------
  private: // Members which are affected by (or support) cropping
    bool parse_hdf5_parameters(jp2_family_tgt &tgt, kdu_args &args);
    void parse_hdf5_metadata(jp2_family_tgt &tgt, bool quiet);
};
