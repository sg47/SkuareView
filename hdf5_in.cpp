/*****************************************************************************/
//
//  @file: hdf5_in.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Sean Peters
//  @date 18/02/2013
//  @brief Implements encoding from HDF5 file format specified by ICRAR to 
//         JPEG2000. Readily extendible to include further features.
//
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
// This code is based on original 
// File: image_in.cpp [scope = APPS/IMAGE-IO]
// Version: Kakadu, V7.0
// Author: David Taubman
// Last Revised: 19 Jan, 2012
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

// System includes
#include <utility>
#include <iostream>
#include <fstream>
#include <string.h>
#include <time.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
// Image includes
#include "kdu_image.h"
// IO includes
#include "kdu_file_io.h"
#include "image_local.h"
#include "hdf5_local.h"
// HDF5 includes
#include "hdf5.h"

/*****************************************************************************/
/* STATIC                  convert_TFLOAT_to_floats                          */
/*****************************************************************************/

static void
convert_TFLOAT_to_floats(float *src, kdu_sample32 *dest,  int num, 
                         bool is_signed, double minval, double maxval, 
                         short domain, std::ofstream& before, 
                         std::ofstream& after)
{
    float factor, scale, offset=0.0;
    float limmin=-0.75, limmax=0.75;
    offset = -0.5;

    /* The images captured in radio astronomy have extremely dynamic range of
     * values. As such a linear scaling will often result an over compressed 
     * image, because most of the data will end up being extremely close 
     * together. As such we use a log or sqrt domain to minimize the loss in 
     * precision */
    if (domain == 0) { // log domain 
        factor = 500;
        scale = 1 / log(fabs(maxval - minval) * factor + 1);
    }
    else if (domain == 1) { // sqrt domain
        scale = 1.0 / sqrt(fabs(maxval - minval));
    }
    else { // linear domain
        scale = 1.0 / fabs(maxval-minval);
    }

    for (int i = 0; i< num; i++)
    {
        float fval;
        if (domain == 0) { // log domain 
            fval = log((src[i] - minval) * factor + 1) * scale + offset;
        }
        else if (domain == 1) { // sqrt domain
            fval = (float)(sqrt(src[i] - minval) * scale + offset);
        }
        else { // linear domain
            fval = src[i] * scale + offset;
        }

        fval = (fval > limmin)?fval:limmin;
        fval = (fval < limmax)?fval:limmax;
        dest[i].fval = fval;

        if (before.is_open() && after.is_open()) {
            before << src[i] << " ";
            after << dest[i].fval << " ";
        }
    }

    if (before.is_open() && after.is_open()) {
        before << std::endl;
        after << std::endl;
    }
}

/*****************************************************************************/
/* STATIC                  convert_TFLOAT_to_ints                          */
/*****************************************************************************/
static void
convert_TFLOAT_to_ints(float *src, kdu_line_buf &line,  int num,
                         int precision, bool is_signed,
                         double minval, double maxval, int bytes_per_sample,
                         std::ofstream& before, std::ofstream& after)
{
    double scale, offset=0.0;
    double limmin=-0.75, limmax=0.75;
   
    scale = 1.0 / fabs(maxval - minval);
    offset = -0.5;

    scale *= (double)((1<<precision)-1);
    offset *= (double)(1<<precision);
    limmin *= (double)(1<<precision);
    limmax *= (double)(1<<precision);

    if (line.get_buf16() != NULL) {
        kdu_sample16* dest = line.get_buf16();
        if (dest == NULL)
            std::cout << "Oh noes" << std::endl;
        // Convert floats to 16 bit integers
        for (int i = 0; i<num; ++i)
        {
            double fval = (double)((src[i] - minval) * scale + offset);
            fval = (fval > limmin)?fval:limmin;
            fval = (fval < limmax)?fval:limmax;
            dest[i].ival = (kdu_int16) fval;
            if (before.is_open() && after.is_open()) {
                before << src[i] << " ";
                after << dest[i].ival << " ";
            }
        }
    }
    else if (line.get_buf32() != NULL)
    { // Convert floats to 32 bit integers
        kdu_sample32* dest = line.get_buf32();
          for (int i = 0; i<num; ++i)
          {
              double fval = (double)((src[i] - minval) * scale + offset);
              fval = (fval > limmin)?fval:limmin;
              fval = (fval < limmax)?fval:limmax;
              dest[i].ival = (kdu_int32) fval;
              if (before.is_open() && after.is_open()) {
                  before << src[i] << " ";
                  after << dest[i].ival << " ";
              }
          }
    }
    else if (bytes_per_sample == 8)
      { // Transfer doubles to ints, with some scaling
        kdu_error e; e << "double to int conversion not implemented";
      }
    else {
      assert(0);
    }

    if (before.is_open() && after.is_open()) {
        before << std::endl;
        after << std::endl;
    }
}

/* ========================================================================= */
/*                                   hdf5_in                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                               hdf5_in::hdf5_in                            */
/*****************************************************************************/

hdf5_in::read_header(jp2_family_tgt &tgt, kdu_args &args)
{
    // Initialize the state incase we need to cleanup prematurely
    num_unread_rows = 0;
    domain=2;
    num_components = 1;
   
    if (!parse_hdf5_parameters(args)) 
        { kdu_error e; e << "Unable to parse HDF5 parameters"; }

    // Open the file
    file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) 
        { kdu_error e; e << "Unable to open input HDF5 file."; }
    
    // Open the dataset
    // TODO: Currently can only read HDF5 images with dataset "full_cube"
    dataset = H5Dopen(file, "full_cube", H5P_DEFAULT); 
    if (dataset < 0) 
        { kdu_error e; e << "Unable to open dataset in HDF5 file."; }
    
    // Get the datatype of the dataset
    datatype = H5Dget_type(dataset);
    if (datatype < 0) 
        { kdu_error e; e << "Unable to get data type of dataset in HDF5 file."; }
    
    // Get the data class of the dataset
    cinfo.t_class = H5Tget_class(datatype);
    if (cinfo.t_class == H5T_NO_CLASS) 
        { kdu_error e; e << "Unable to get class type of dataset in HDF5 file."; }

    // Get the data order (i.e littlendian or bigendian)
    order = H5Tget_order(datatype); 
    if (order == H5T_ORDER_LE)
        littlendian = true;
    else if (order == H5T_ORDER_BE)
        littlendian = false;
    else
        { kdu_error e; e << "Unable to identify data order of dataset in HDF5 "
                            "file."; }
    
    // Identify if the data is signed
    is_signed = true; // floats are always signed
    if (cinfo.t_class != H5T_FLOAT) { // TODO: When implement support for further
                                      // classes. This will be extended.
        int is_signed_h5 = H5Tget_sign(datatype);
        if (is_signed_h5 == H5T_SGN_2)
            is_signed = true;
        else if (is_signed_h5 == H5T_SGN_NONE)
            is_signed = false;
        else
            { kdu_error e; e << "Unable to identify is data is signed."; }
    }

    // Get the number of bytes that represent each sample
    bytes_per_sample = H5Tget_size(datatype); 
    if (bytes_per_sample == 0) 
        { kdu_error e; e << "Unable to get sample bytes of dataset in HDF5 "
                            "file."; }
    
    // Get the number of bits that represent each sample
    precision = H5Tget_precision(datatype);
    if (precision == 0) 
        { kdu_error e; e << "Unable to get precision of dataset in HDF5 file."; }
    else if (precision != 8 * bytes_per_sample)
        { kdu_error e; e << "Padding in sample bytes. Handling for this is "
                            "unimplemented"; } 

    // Check for forced precision
    if (forced_prec > 0) {
        precision = forced_prec;
        bytes_per_sample = precision / 8;
    }

    // Get the dataspace of the the dataset
    dataspace = H5Dget_space(dataset);
    if (dataspace < 0) 
        { kdu_error e; e << "Unable to get dataspace of dataset in HDF5 "
                            "file."; }
    
    // Get the rank (number of dims)
    cinfo.naxis = H5Sget_simple_extent_ndims(dataspace);
    if (cinfo.naxis < 0) 
        { kdu_error e; e << "Unable to get number of dimensions of the "
                            "dataspace of dataset in HDF5 file."; }
                            
    // Get the extent of the dimensions of the dataspace
    hsize_t* dims_dataset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);     
    if (H5Sget_simple_extent_dims(dataspace, dims_dataset, NULL) != cinfo.naxis)
    { kdu_error e; e << "Unable to get dimensions of the dataspace of "
                        "dataset in HDF5 file."; }
       
    /* Input dimensions (FREQ,DEC,RA) as (x,y,z)
     * Output  dimensions (RA,DEC,FREQ) as (x,y,z) */
    cinfo.width = (unsigned long)dims_dataset[2];
    cinfo.height = (unsigned long)dims_dataset[1];
    cinfo.depth = (unsigned long)dims_dataset[0];
        
    std::cout << "HDF5 image dimensions:\n"
           "rank = " << (unsigned int)(cinfo.naxis) << "\n" << 
           "rows = " << (unsigned int)(cinfo.height) << "\n" << 
           "cols = " << (unsigned int)(cinfo.width) << "\n";  
    free(dims_dataset);

    // Now we handle the cropping parameter

    // Offset of cube to be encoded within the original file image
    offset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    // Extent of each dimension of the cube to be encoded
    extent = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    
    if (crop.specified) {
        if ((crop.x + crop.width) > cinfo.width)
            { kdu_error e; e << "Requested input file cropping parameters are "
            "not compatible with actual image dimensions.  The cropping "
            "region would cross the right hand boundary of the image."; }
        if ((crop.y + crop.height) > cinfo.height)
            { kdu_error e; e << "Requested input file cropping parameters are "
            "not compatible with actual image dimensions. The cropping "
            "region would cross the the lower hand boundary of the image."; }
        offset[0] = crop.x; extent[0] = crop.width;         
        offset[1] = crop.y; extent[1] = crop.height;
        offset[2] = crop.z; extent[2] = 1;
    }
    else { // No cropping specified, default is the whole image
        offset[0] = offset[1] = offset[2] = 0;
        extent[0] = cinfo.width;
        extent[1] = cinfo.height;
        extent[2] = 1;
    }

    // Define the memory space that will be used by get
    // Each call of get returns an image row. So memspace needs to be the size
    // of a row
    dims_mem = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    for (int i = 0; i < cinfo.naxis; ++i)
        dims_mem[i] = 1; // all the other dimensions are done one at a time
    dims_mem[2] = extent[0]; // read in all the cols (i.e an entire row)
                             // remember in the z dimension in hyperslab
                             // defined by dims_mem[2] will be mapped to our
                             // output x dimension.

    memspace = H5Screate_simple(cinfo.naxis, dims_mem, NULL);
    if (memspace < 0)
        { kdu_error e; e << "Unable to create dataspace (memspace)."; }

    // The offset where we place the hyperslab in memory is 0,0,0,...
    // anything else would just be a waste of memory.
    hsize_t* offset_mem = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    for (int i = 0; i < cinfo.naxis; ++i)
        offset_mem[i] = 0;
    if (H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset_mem, NULL,
        dims_mem, NULL) < 0)
        { kdu_error e; e << "Unable to create hyperslab in memory"; }
    free(offset_mem);

    int num_colours = 1;
    int colour_space_confidence = 0;
    jp2_colour_space colour_space = JP2_sLUM_SPACE;
    bool has_premultiplied_alpha = false;
    bool has_unassociated_alpha = false;

    if (next_comp_idx == 0)
        dims.set_colour_info(num_colours,
                             has_premultiplied_alpha,
                             has_unassociated_alpha,
                             colour_space_confidence,
                             colour_space);

    first_comp_idx = next_comp_idx;

    // Add components
    for (int n = 0; n < num_components; ++n) {
        dims.add_component(extent[1], extent[0], precision, is_signed,
                next_comp_idx);
        next_comp_idx++;
    }

    offset_out = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    for (int i = 0; i < cinfo.naxis; ++i) {
        offset_out[i] = offset[i];
    }

    if (cinfo.naxis > 3)
        num_unread_rows = extent[1] * extent[2] * extent[3];
    else if (cinfo.naxis == 3)
        num_unread_rows = extent[1] * extent[2];
    else
        num_unread_rows = extent[1]; 
    total_rows = num_unread_rows;
} 


/*****************************************************************************/
/*                               hdf5_in::~hdf5_in                           */
/*****************************************************************************/

hdf5_in::~hdf5_in()
{
    if ((num_unread_rows > 0) || (incomplete_lines != NULL))
    { kdu_warning w;
        w << "Not all rows of image component "
        << first_comp_idx << " were consumed!";
    }
    
    free(offset);
    free(extent);
    free(dims_mem);
    free(offset_out);
    
    if (H5Tclose(datatype) < 0 || 
        H5Dclose(dataset) < 0 ||
        H5Sclose(dataspace) < 0 || 
        H5Sclose(memspace) < 0 || 
        H5Fclose(file) < 0)
        { kdu_error e; e << "Unable to close HDF5 file succesflly."; }
}

/*****************************************************************************/
/*                                 hdf5_in::get                              */
/*****************************************************************************/

bool
hdf5_in::read_stripe(int height, kdu_int32 *buf)
/* Reads in a stripe from the image and places it into buf. We make the rather
 * dangerous assumption that the stripe height provided will never exceed the
 * bounds of the image from our current index in the cube. */
{
  int width = crop.width; // Number of samples in the line
  dims_mem[1] = height;
    
  // We select the hyperslab (cropped image cube) that will be encoded

  // Select the length of the z dimension of the hdf5 image, this will become 
  // a row (x dim) in our jpeg2000 image.

  std::swap (offset_out[0], offset_out[2]);
  if (H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset_out, NULL, 
              dims_mem, NULL) < 0)
      { kdu_error e; e << "Unable to select cropped hyperslab of dataset in"
                          "HDF5 file."; }
  std::swap (offset_out[0], offset_out[2]);

  // TODO: extend to all types (don't have other test data at the moment.
  switch (cinfo.t_class) {       
    case H5T_FLOAT: { 
      // We finally actually read the values from the HDF5 image.
      float* buffer = (float*) malloc(sizeof(float)*width);

      if (H5Dread(dataset, H5T_NATIVE_FLOAT, memspace, dataspace,
                  H5P_DEFAULT, buffer) < 0)
        { kdu_error e; e << "Unable to read FLOAT HDF5 dataset."; }

      if (line.is_absolute())
        convert_TFLOAT_to_ints(buffer, line, width,
                  precision, true, float_minvals, float_maxvals, 
                  bytes_per_sample, raw_before, raw_after);
      else
        convert_TFLOAT_to_floats(buffer, line.get_buf32(), width,
            is_signed, float_minvals, float_maxvals, domain, 
              raw_before, raw_after);

      free(buffer);
      break;
    }
    default: 
      kdu_error e; e << "Unimplemented class type."; 
      break; 
  }
      
  /* Incremement position in HDF5 file
   * Indices represent:
   * 0 col
   * 1 row
   * 2 frame */
  offset_out[0] = offset[0]; // set col to beginning of next line
  if (offset_out[1] == offset[1] + extent[1] - 1) { // just read last row in frame
      offset_out[1] = offset[1]; // set row to beginning of next frame
      if (cinfo.naxis > 2 && cinfo.depth > 1) {
          if (offset_out[2] != offset[2] + extent[2] - 1) {
              offset_out[2]++; // next frame
              ++comp_idx;      // new frame - next component
          }
      }
  }
  else {
      offset_out[1] += height; // otherwise just go to next row
  }
  num_unread_rows -= height;
}

/*****************************************************************************/
/*                     hdf5_in::parse_hdf5_parameters                        */
/*****************************************************************************/

bool hdf5_in::parse_hdf5_parameters(jp2_family_tgt &tgt, kdu_args &args)
{
  if (args.get_first() != NULL) {
    if (args.find("-domain") != NULL)
    {
      const char *string = args.advance();

      if (strcmp("log",string) == 0)
        domain=0;
      else if (strcmp("sqrt",string) == 0)
        domain=1;
      else
        domain=2; // linear
      args.advance();
    }
    return true;
  }

