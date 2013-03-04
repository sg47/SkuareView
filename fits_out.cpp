/*****************************************************************************/
//
//  @file: fits_out.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Slava Kitaeff
//  @date 29/07/12.
//  @brief Implements file writing to the FITS file format, provides support
//         up to 4 dimensions and is readily extendible to include further
//         features.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/******************************************************************************/

// System includes
#include <iostream>
#include <string.h>
#include <math.h>
#include <assert.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
// Image includes
#include "kdu_image.h"
#include "image_local.h"
#include "fits_local.h"
// Fits includes
#include "fitsio.h"

/* ========================================================================= */
/*                                 fits_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                             fits_out::fits_out                            */
/*****************************************************************************/

fits_out::fits_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
                   bool quiet)
{
    // Initialize state information in case we have to clean up prematurely
    orig_precision = NULL;
    is_signed = NULL;
    incomplete_lines = NULL;
    free_lines = NULL;
    num_unwritten_rows = 0;
    initial_non_empty_tiles = 0;

    /* Retrieve and use varaibles related to the input JPX image */

    // Find max image components
    first_comp_idx = next_comp_idx;
    num_components = dims.get_num_components() - first_comp_idx;
    if (num_components <= 0)
        { kdu_error e; e << "Output image files require more image components "
          "(or mapped colour channels) than are available!"; }

    cinfo.bitpix = FLOAT_IMG;
    cinfo.naxis = 2;
    if (num_components > 1)
        cinfo.naxis++;
    
    // As far as I understand there is no way to determine at this point in
    // the code, how many tiles we have without editing the kakadu libarary,
    // as such I have only provided support for decoding FITS images with up to
    // 3 dimensions.
    if (false) // Fourth dimension (stokes)
        cinfo.naxis++;


    // Create destination FITS file
    fits_create_file(&out, fname, &status);
    
    fits_create_img(&out, FLOAT_IMG, 
    
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
}

/*****************************************************************************/
/*                               fits_out::put                               */
/*****************************************************************************/

void
    fits_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
}


