/*****************************************************************************/
//  @file: hdf5_in.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Sean Peters
//  @date 25/01/2013
//  @brief TODO
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
/******************************************************************************
Description:
   Implements image file reading for a variety of different file formats:
currently BMP, PGM, PPM, TIFF and RAW only.  Readily extendible to include
other file formats without affecting the rest of the system.
******************************************************************************/

// System includes
#include <iostream>
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
                         bool is_signed, double minval, double maxval)
{
    float scale, offset=0.0;
    float limmin=-0.75, limmax=0.75;
    if (is_signed)
        scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
    else
    {
        scale = 1.0 / maxval;
        offset = -0.5;
    }

    for (int i = 0; i< num; i++)
    {
        float fval = (float)(src[i] * scale + offset);
        fval = (fval > limmin)?fval:limmin;
        fval = (fval < limmax)?fval:limmax;
        dest[i].fval = fval;
    }
}

/*****************************************************************************/
/* STATIC                  convert_TFLOAT_to_ints                          */
/*****************************************************************************/
static void
convert_TFLOAT_to_ints(float *src, kdu_sample32 *dest,  int num,
                         int precision, bool is_signed,
                         double minval, double maxval, int sample_bytes)
{
    double scale, offset=0.0;
    double limmin=-0.75, limmax=0.75;
    if (is_signed)
      scale = 0.5 / (((maxval+minval) > 0.0)?maxval:(-minval));
    else
      {
        scale = 1.0 / maxval;
        offset = -0.5;
      }
    scale *= (double)((1<<precision)-1);
    offset *= (double)(1<<precision);
    limmin *= (double)(1<<precision);
    limmax *= (double)(1<<precision);
    if (sample_bytes == 4)
      { // Transfer floats to ints
          for (int i = 0; i<num; ++i)
            {
              double fval = (float)(src[i] * scale + offset);
              fval = (fval > limmin)?fval:limmin;
              fval = (fval < limmax)?fval:limmax;
              dest[i].ival = (kdu_int32) fval;
            }
      }
    else if (sample_bytes == 8)
      { // Transfer doubles to ints, with some scaling
        kdu_error e; e << "double to int conversion not implemented";
      }
    else
      assert(0);
}

/* ========================================================================= */
/*                                   hdf5_in                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                               hdf5_in::hdf5_in                            */
/*****************************************************************************/

hdf5_in::hdf5_in(const char *fname, 
                 kdu_args &args, 
                 kdu_image_dims &dims, 
                 int &next_comp_idx,
                 bool &vflip,
                 kdu_rgb8_palette *palette)
{
    // Initialize the state incase we need to cleanup prematurely
    incomplete_lines = NULL;
    free_lines = NULL;
    num_unread_rows = 0;
   
    if (!parse_hdf5_parameters(args)) 
        { kdu_error e; e << "Unable to parse HDF5 parameters"; }

    // Open the file
    file = H5Fopen(fname, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file < 0) 
        { kdu_error e; e << "Unable to open input HDF5 file."; }
    
    // Open the dataset
    dataset = H5Dopen(file, "full_cube", H5P_DEFAULT); // TODO: dynamic data set name
                                                       // this would conflict with
                                                       // min/max idea
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
    
    // TODO: should use a bool rather than the HDF5 datatype
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
    sample_bytes = H5Tget_size(datatype); 
    if (sample_bytes == 0) 
        { kdu_error e; e << "Unable to get sample bytes of dataset in HDF5 "
                            "file."; }
    
    // Get the number of bits that represent each sample
    precision = H5Tget_precision(datatype);
    if (precision == 0) 
        { kdu_error e; e << "Unable to get precision of dataset in HDF5 file."; }
    else if (precision != 8 * sample_bytes)
        { kdu_error e; e << "Padding in sample bytes. Handling for this is "
                            "unimplemented"; } // TODO

    // TODO: implement forced precision
    bool align_lsbs = false;
    int forced_prec = dims.get_forced_precision(next_comp_idx, align_lsbs);
    if (forced_prec > 0)
        precision = forced_prec;

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
    hsize_t* dims_dataset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis); // Dataset dimensions
    if (H5Sget_simple_extent_dims(dataspace, dims_dataset, NULL) != cinfo.naxis)
    { kdu_error e; e << "Unable to get dimensions of the dataspace of "
                        "dataset in HDF5 file."; }
       
    // Input dimensions (FREQ,DEC,RA) as (x,y,z)
    // Output  dimensions (RA,DEC,FREQ) as (x,y,z) 

    cinfo.width = (unsigned long)dims_dataset[2];
    cinfo.height = (unsigned long)dims_dataset[1];
        
    std::cout << "HDF5 image dimensions:\n"
           "rank = " << (unsigned int)(cinfo.naxis) << "\n" << 
           "rows = " << (unsigned int)(cinfo.height) << "\n" << 
           "cols = " << (unsigned int)(cinfo.width) << "\n";  
   
    if (cinfo.naxis == 3) {
        cinfo.depth = (unsigned long)dims_dataset[0];
        std::cout << "frames = " << (unsigned int)(cinfo.depth) << "\n";
    }
    if (cinfo.naxis == 4) {
        cinfo.stokes = (unsigned long)dims_dataset[3];
        std::cout << "stokes = " << (unsigned int)(cinfo.stokes) << "\n";
    }
    
    free(dims_dataset);

    // Now we handle the cropping parameter
    // Note: This parameter will only handle 2 dimensions of cropping, we have
    // to handle the other(s) seperately.
    
    int crop_y, crop_x, crop_height, crop_width;
    // Offset of cube to be encoded within the original file image
    offset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    // Extent of each dimension of the cube to be encoded
    extent = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    
    if (dims.get_cropping(crop_y, crop_x, crop_height, crop_width, next_comp_idx)) {
        if ((crop_x < 0) || (crop_y < 0)) 
            { kdu_error e; e << "Requested input file cropping parameters are "
            "illegal -- cannot have negative cropping offsets."; }
        if ((crop_x + crop_width) > cinfo.width)
            { kdu_error e; e << "Requested input file cropping parameters are "
            "not compatible with actual image dimensions.  The cropping "
            "region would cross the right hand boundary of the image."; }
        if ((crop_y + crop_height) > cinfo.height)
            { kdu_error e; e << "Requested input file cropping parameters are "
            "not compatible with actual image dimensions. The cropping "
            "region would cross the the lower hand boundary of the image."; }
        offset[0] = crop_x;     offset[1] = crop_y;
        extent[0] = crop_width;
        extent[1] = crop_height; 
    }
    // TODO: Documentation on get_cropping is poor, as such if cropping has not been
    // specified it is only my current assumption that it will return false
    // I currently can't test the above as my only example file is over 1TB
    else { // No cropping specified, default is the whole image
        offset[0] = offset[1] = 0;
        extent[0] = cinfo.width;
        extent[1] = cinfo.height;
    }

    // If we have 3 dimensions
    if (cinfo.naxis > 2) {
        // No cropping was specified on this dimension
        if (h5_param.end_frame == 0) {
            extent[2] = cinfo.depth;
            offset[2] = 0;
        }
        // Cropping was specified on this dimension
        else {
            extent[2] = abs(h5_param.end_frame - h5_param.start_frame);
            offset[2] = h5_param.start_frame;
        }
        if (cinfo.depth < extent[2])
            { kdu_error e; e << "The number of available frames in the HDF5 file"
            " is less than requested."; }        
        num_components = extent[2]; 
    }
    else {
        num_components = 1;
    }
    
    // If we have 4 or more dimensions
    if (cinfo.naxis > 3) { 
        // No cropping was specified on this dimension
        if (h5_param.end_stoke == 0) { 
            offset[3] = 0;
            extent[3] = cinfo.stokes;
        }
        // Cropping was specified on this dimension
        else {
            extent[3] = abs(h5_param.end_stoke - h5_param.start_stoke); offset[3] = h5_param.start_stoke;
        }
        if (cinfo.stokes < extent[3]) 
            { kdu_error e; e << "The number of available stokes in the HDF5 "
            "files is less than requested."; }
        for (int i = 4; i < cinfo.naxis; ++i) {
            offset[i] = 0;
            extent[i] = 1;
            if (dims_dataset[i] > 1)
                { kdu_error e; e << "Dimension " << i+1 << " has a length "
                " greater than 1."; }
        }
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
    image_line_buf *tmp;
    while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
    while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }
    
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
hdf5_in::get(int comp_idx, // component index. We use components for frames
             kdu_line_buf &line, 
             int x_tnum  // tile number, starts from 0. We use tiles for stokes
             )
{
    // Progress 
    //if (num_unread_rows % (total_rows / 100) == 0 && num_unread_rows != 0)
        //std::cout << 100 * (total_rows - num_unread_rows) / total_rows << "%\n";

    // In the context of this message the hyperslab will just be the line.
    int width = line.get_width(); // Number of samples in the line
    assert((comp_idx >= 0) && (comp_idx < num_components));
    
    image_line_buf *scan, *prev = NULL;
    for (scan = incomplete_lines; scan != NULL; prev = scan, scan = scan->next) {
        assert(scan->next_x_tnum >= x_tnum);
        if (scan->next_x_tnum == x_tnum)
            break;
    }
    if (scan == NULL) {
        // Need to read a new image line.
        assert(x_tnum == 0);
        if (num_unread_rows == 0)
           return false;
        
        if ((scan = free_lines) == NULL)
            scan = new image_line_buf(width, sample_bytes);

        // Big enough for padding and expanding bits to bytes
        free_lines = scan->next;
        if (prev == NULL)
            incomplete_lines = scan;
        else
            prev->next = scan;
        scan->accessed_samples = 0;
        scan->next_x_tnum = 0;
    
        // Class of input dataset
        // Currently reading row by row
        
        // We select the hyperslab (cropped image cube) that will be encoded

        // Select the length of the z dimension, this will become a row (x dim)
        // in our image.

        //std::cout << std::endl;
        //for (int i = 0; i < 3; ++i) 
            //std::cout << offset_out[i] << " for " << dims_mem[i] << std::endl;

        hsize_t temp = offset_out[0];
        offset_out[0] = offset_out[2];
        offset_out[2] = temp;
        if (H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset_out, NULL, dims_mem, 
                    NULL) < 0)
            { kdu_error e; e << "Unable to select cropped hyperslab of dataset in"
                                "HDF5 file."; }
        temp = offset_out[0];
        offset_out[0] = offset_out[2];
        offset_out[2] = temp;

        switch (cinfo.t_class) { // TODO: extend to all types (don't have other 
                                 // test data at the moment.
            case H5T_FLOAT: { 
                // We finally actually read the values from the HDF5 image.
                float* buffer = (float*) malloc(sizeof(float)*width);

                if (H5Dread(dataset, H5T_NATIVE_FLOAT, memspace, dataspace,
                            H5P_DEFAULT, buffer) < 0)
                    { kdu_error e; e << "Unable to read FLOAT HDF5 dataset."; }

                if (line.is_absolute())
                    convert_TFLOAT_to_ints(buffer, line.get_buf32(), width,
                            precision, true, float_minvals, float_maxvals, sample_bytes);
                else
                    convert_TFLOAT_to_floats(buffer, line.get_buf32(), width,
                        is_signed, float_minvals, float_maxvals);

                // TODO: HDF5 has no way of determining the min or max float value
                // this is important for normalizing the floats for Kadaku. The
                // below is a way to get the values in a trial and then hardcode
                // for the next use.
                //for (int i = 0; i < width; ++i) {
                //    if (buffer[i] < min)
                //      min = buffer[i];
                //    if (buffer[i] > max)
                //        max = buffer[i];
                //}
                
                free(buffer);
                break;
            }
            default: 
                kdu_error e; e << "Unimplemented class type."; 
                break; 
        }
        
        // Incremement position in HDF5 file
        // Indices represent:
        // 0 col
        // 1 row
        // 2 frame
        // 3 stoke
        offset_out[0] = offset[0]; // set col to beginning of next line
        if (offset_out[1] == offset[1] + extent[1] - 1) { // just read last row in frame
            offset_out[1] = offset[1]; // set row to beginning of next frame
            if (cinfo.naxis > 2 && cinfo.depth > 1) {
                if (offset_out[2] != offset[2] + extent[2] - 1) {
                    offset_out[2]++; // next frame
                    ++comp_idx;      // new frame - next component
                }
                else if (cinfo.naxis > 3 && cinfo.stokes > 1) {
                    offset_out[2] = offset[2]; // set to beginning of
                                               // next stoke
                    offset_out[3]++; // new stoke
                    scan->next_x_tnum++; // next tile
                }
            }
        }
        else {
            offset_out[1]++; // otherwise just go to next row
        }
        num_unread_rows--;
    }

    assert((scan->width - scan->accessed_samples) >= width);
    scan->accessed_samples += scan->width;
    if (scan->accessed_samples == scan->width) {
        assert(scan == incomplete_lines);
        incomplete_lines = scan->next;
        scan->next = free_lines;
        free_lines = scan;
    }
    return true;
}

/*****************************************************************************/
/*                     hdf5_in::parse_hdf5_parameters                        */
/*****************************************************************************/

bool hdf5_in::parse_hdf5_parameters(kdu_args &args) {
    const char* string;

    if (args.get_first() != NULL) {
        
        if (args.find("-minmax") != NULL)
        {
            for (int i = 0; i < 2; ++i) {
                const char *string = args.advance();
                bool succ = true;
                for (int j = 0; j < strlen(string); ++j) {
                    if (! (std::isdigit(string[j]) || 
                                string[j] == '.' || string[j] == '-'))
                        succ = false;
                }
                if (!succ || (i == 0 && (sscanf(string, "%f", &float_minvals) != 1)))
                    succ = false;
                else if (!succ || (i == 1 && 
                            (sscanf(string, "%f", &float_maxvals) != 1)))
                    succ = false;
                
                if (!succ)
                     { kdu_error e; e << "\"-minmax\" argument contains "
                        "malformed specification. Expected to find two comma-"
                        "separated float numbers, enclosed by curly braces. "
                        "Example: -minmax {-1.0,1.0}"; }
            }
            args.advance();
        }
        // TODO: This is hardcoded in just to testing simpler
        else {
           float_minvals = -0.00139705;
           float_maxvals = 0.00171688;
        }

        if (args.find("-iplane") != NULL)
        {
            const char *field_sep, *string = args.advance();
            for (field_sep=NULL; string != NULL; string=field_sep)
            {
                std::cout << string << std::endl;
                if (field_sep != NULL)
                {
                    if (*string != ',')
                    { kdu_error e; e << "\"-iplane\" argument requires a comma-"
                        "separated first and last plane parameters to read."; }
                    string++; // Walk past the separator
                }
                if (*string == '\0')
                    break;
                if (((field_sep = strchr(string,'}')) != NULL) &&
                    (*(++field_sep) == '\0'))
                    field_sep = NULL;
                
                if ((sscanf(string,"{%ld,%ld}", &h5_param.start_frame,
                            &h5_param.end_frame) != 2) || (h5_param.start_frame < 1) || 
                    (h5_param.end_frame < 1) || (h5_param.start_frame >= h5_param.end_frame))
                { kdu_error e; e << "\"-iplane\" argument contains malformed "
                    "plane specification.  Expected to find two comma-separated "
                    "integers, enclosed by curly braces.  They must be strictly "
                    "positive integers. The second parameter must be larger "
                    "than the first one"; }
            }
            args.advance();
        }        

        if (args.find("-istok") != NULL)
        {
            const char *field_sep, *string = args.advance();
            for (field_sep=NULL; string != NULL; string=field_sep)
            {
                if (field_sep != NULL)
                {
                    if (*string != ',')
                    { kdu_error e; e << "\"-istok\" argument requires a comma-"
                        "separated first and last Stok parameters to read."; }
                    string++; // Walk past the separator
                }
                if (*string == '\0')
                    break;
                if (((field_sep = strchr(string,'}')) != NULL) &&
                    (*(++field_sep) == '\0'))
                    field_sep = NULL;

                if ((sscanf(string,"{%ld,%ld}", &h5_param.start_stoke,
                            &h5_param.end_stoke) != 2) ||
                    (h5_param.start_stoke < 1) || (h5_param.end_stoke < 1) || 
                    (h5_param.start_stoke <= h5_param.end_stoke) || 
                    (h5_param.start_stoke > 5) ||
                    (h5_param.end_stoke > 5))
                { kdu_error e; e << "\"-istok\" argument contains malformed "
                    "stok specification.  Expected to find two comma-separated "
                    "integers, enclosed by curly braces.  They must be strictly "
                    "parameter must be larger than the first one"; }
            }
            args.advance();
        }
    }

    return true;
}

