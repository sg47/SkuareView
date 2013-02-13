/*****************************************************************************/
//  
//  @file: hdf5_local.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Slava Kitaeff
//  @date 29/07/12.
//  @brief The The file contains the definitions of types and classes
//  @brief for the HDF5 image format.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/*****************************************************************************/

// Specific to ICRAR's hdf5 image format.
#define DATASET_NAME "full_cube"
//#define H5_FLOAT_MIN -.006383
//#define H5_FLOAT_MAX 0.105909
//#define H5_FLOAT_MIN -0.000354053
//#define H5_FLOAT_MAX 0.00106955
//frame 407 - 500x500 min max

#define H5_FLOAT_MIN -0.000611
#define H5_FLOAT_MAX 0.000553

#include <stdio.h> // C I/O functions can be quite a bit faster than C++ ones
#include "kdu_elementary.h"
#include "kdu_image.h"
#include "kdu_file_io.h"
#include "hdf5.h"

typedef struct {
    long start_frame;// First frame of data cube to read.  Ignored for 2D images.  
    // Will only be modified if the x parameter is present.  
    // If only x is specified (and not y), the single x
    //value will be interpreted as a single frame to read.
    long end_frame;  // Last frame of data cube to read.  Ignored for 2D images.  
    // Will only be modified if the y parameter is present.
    long start_stoke; // startStoke First stoke of data volume to read.  Ignored 
    // for 2D or 3D images.  Will only be modified if the S1 
    // parameter is present.  If only S1 is specified (and not 
    // S2), the single S1 value will be interpreted as a single 
    // stoke to read.
    long end_stoke; // Last stoke of data volume to read.  Ignored for 2D or 3D 
    // images.  Will only be modified if the S2 parameter is 
    // present.
} hdf5_param;

/**
 * Structure for defining essential properties of a HDF5 datacube.
 */
typedef struct {
    long width; // Image width
    long height; // Image height
    long depth; // Image depth. Arbitrary for 2D images
    long stokes; // Number of stokes in image. Arbitrary for 2D or 3D images
    int naxis;
    int t_class; // Image data type.  Similar to BITPIX in CFITSIO
}  hdf5_cube_info;

class hdf5_in;

/*****************************************************************************/
/*                             class hdf5_in                                 */
/*****************************************************************************/

class hdf5_in : public kdu_image_in_base {
public: // Member functions
    hdf5_in(const char *fname, kdu_args &args, kdu_image_dims &dims,  
            int &next_comp_idx, bool &vflip, kdu_rgb8_palette *palette);
    ~hdf5_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
private: // Members describing the organization of the FITS data
    hdf5_param h5_param;;
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
    hsize_t* extent; // The extent of each of the dimensions of the hyperslab in 
                     // the file. i.e. length, breadth, etc.

    hdf5_cube_info cinfo;
        
    double float_minvals; // When HDF5 file contains floating-point samples
    double float_maxvals; // When HDF5 file contains floating-point samples
    //kdu_uint16 bitspersample;
    kdu_simple_file_source src;
   
    bool is_signed; // Whether the data is signed or not
    bool littlendian; // true if data order is littlendian
    int first_comp_idx;
    int num_components; // May be > `samplesperpixel' if there is a palette
    int precision;
    int sample_bytes; // After expanding any palette
    int num_unread_rows; // Always starts at `rows', even with cropping
    int total_rows; // Used for progress bar
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    //--------------------------------------------------------------------------
private: // Members which are affected by (or support) cropping
    bool parse_hdf5_parameters(kdu_args &args, kdu_image_dims &dims);
};

/*****************************************************************************/
/*                             class hdf5_out                                */
/*****************************************************************************/

class hdf5_out : public kdu_image_out_base {
  public: // Member functions
    hdf5_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
            bool quiet);
    ~hdf5_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    hdf5_cube_info cinfo; // Contains the essential structural information for
                          // an HDF5 image cube
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

    int first_comp_idx;
    int num_components;
    int precision;
    float float_minvals, float_maxvals;
    bool forced_align_lsbs;
    int *orig_precision; // All equal to above unless `precision' is forced.
    bool *is_signed; // One entry for each component
    int scanline_width, unpacked_samples;
    int sample_bytes, pixel_bytes, row_bytes;
    bool pre_pack_littlendian; // Scanline byte order prior to packing
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unwritten_rows;
    kdu_simple_file_target out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
    //--------------------------------------------------------------------------
private: // Members which are affected by (or support) cropping
    bool parse_hdf5_parameters(kdu_args &args);
    void parse_hdf5_metadata(kdu_image_dims &dims, bool quiet);
};
