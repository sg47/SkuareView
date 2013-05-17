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
  kdu_error e;
  orig_precision = NULL;
  is_signed = NULL;
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unwritten_rows = 0;

  /* Retrieve and use varaibles related to the input JPX image */

  // Find max image components
  first_comp_idx = next_comp_idx;
  num_components = dims.get_num_components() - first_comp_idx;
  if (num_components <= 0)
    e << "Output image files require more image components " 
      "(or mapped colour channels) than are available!";

  cinfo.bitpix = FLOAT_IMG;
  cinfo.naxis = 2;
  if (num_components > 1)
    cinfo.naxis++;
  
  // Create destination FITS file
  fits_create_file(&out, fname, &status);
  
  int* naxes = (int*) malloc(sizeof(int) * cinfo.naxis);
  naxes[0] = cinfo.width;
  naxes[1] = cinfo.height;

  fits_create_img(&out, cinfo.bitpix, cinfo.naxis, &status);
  if (status != 0) e << "Unable to create FITS image.";
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
  if (fits_close_file( 
}

/*****************************************************************************/
/*                               fits_out::put                               */
/*****************************************************************************/

void
    fits_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int width = line.get_width();
  int idx = comp_idx - this->first_comp_idx;

  // ICRAR's current HDF5 image format makes no use of tiles. So much of the
  // tile related code, may be considered a little superfluous and is 
  // completely untested. However I thought to include it just in case.
  x_tnum = x_tnum * num_components + idx; 

  if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles)) {
    assert(width == 0);
    return;
  }

  image_line_buf *scan=NULL, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next) {
    assert(scan->next_x_tnum >= x_tnum);
    if (scan->next_x_tnum == x_tnum)
      break;
  }

  if (scan == NULL) { // Need to open a new line buffer.
    assert(x_tnum == 0); // Must supply samples from left to right.
    if ((scan = free_lines) == NULL)
      // Big enough for padding and expanding bits to bytes
      scan = new image_line_buf(width, sample_bytes);
    free_lines = scan->next;
    if (prev == NULL)
      incomplete_lines = scan;
    else
      prev->next = scan;
    scan->accessed_samples = 0;
    scan->next_x_tnum = 0;
  }
  assert((scan->width-scan->accessed_samples) >= line.get_width());

  scan->next_x_tnum++; 
  if (idx == (num_components-1))
    scan->accessed_samples += line.get_width();
  if (scan->accessed_samples == cinfo.width) {
    // Write completed line and send it to the free list
    if (initial_non_empty_tiles == 0)
      initial_non_empty_tiles = scan->next_x_tnum;
    else
      assert(initial_non_empty_tiles == scan->next_x_tnum);

    if (num_unwritten_rows == 0)
    { kdu_error e; e << "Attempting to write too many lines to image "
      "file for components " << first_comp_idx << " through "
        << first_comp_idx + num_components - 1 << "."; }

      // Select the hyperslab (in this case row) that we are going to write to
      if (H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL,
            dims_mem, NULL) < 0)
      { kdu_error e; e << "Unable to select hyperslab within HDF5 dataset."; }

      // Finall we write the row to HDF5 file
      float* buf = (float*) malloc(4 * width); //TODO: With other formats this
      // buffer size will change.
      if (line.is_absolute()) {
        convert_ints_to_TFLOAT(line, buf, width,
            orig_precision[idx], float_minvals, float_maxvals,
            raw_before, raw_after);

        if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
              H5P_DEFAULT, buf) < 0)
        { kdu_error e; e << "Unable to write to HDF5 file."; }
      }
      else {
        convert_floats_to_TFLOAT(line.get_buf32(), buf, width,
            float_minvals, float_maxvals, domain, raw_before,
            raw_after);

        if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
              H5P_DEFAULT, buf) < 0)
        { kdu_error e; e << "Unable to write to HDF5 file."; }
      }
      free(buf);

      // Adjust our offset in the image after writing the row
      // TODO: currently specifiying a cropping on the image is unimplemented.
      offset[0] = 0; // set col to beginning of next line
      if (offset[1] == dest_dims[1] - 1) { // just read last row in frame
        offset[1] = 0; // set row to beginning of next frame
        if (cinfo.naxis > 2 && cinfo.depth > 1) {
          if (offset[2] != dest_dims[2] - 1) {
            offset[2]++; // next frame
            ++comp_idx;      // new frame - next component
          }
        }
      }
      else {
        offset[1]++; // otherwise just go to next row
      }

      num_unwritten_rows--;
      assert(scan == incomplete_lines);
      incomplete_lines = scan->next;
      scan->next = free_lines;
      free_lines = scan;
  }
}


