/*****************************************************************************/
//
//  @file: fits_out.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Sean Peters
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
/*                           fits_out::write_header                          */
/*****************************************************************************/

void
fits_out::write_header(jp2_family_src &src, kdu_args &args, 
    ska_dest_file* const dest_file)
{
  // Initialize state information in case we have to clean up prematurely
  //TODO: make dynamic
  dest_file->is_signed = true;
  dest_file->precision = 32;
  dest_file->reversible = false;
  dest_file->bytes_per_sample = 4;
  num_unwritten_rows = 0;

  fpixel = new LONGLONG [naxis];
  fpixel[0] = dest_file->crop.x+1;
  fpixel[1] = dest_file->crop.y+1;
  std::cout << fpixel[0] << " " << fpixel[1] << std::endl;
  /* Retrieve and use varaibles related to the input JPX image */

  bitpix = FLOAT_IMG;
  naxis = 2; 
  long* naxes = new long [naxis];
  naxes[0] = dest_file->crop.width;
  naxes[1] = dest_file->crop.height;

  // Create destination FITS file
  fits_create_file(&out, dest_file->fname, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to create FITS file."; }

  int iomode = READWRITE;
  fits_file_mode(out, &iomode, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to write to FITS file."; }

  std::cout << naxis << std::endl;
  std::cout << naxes[0] << " " << naxes[1] << std::endl;
  fits_create_img(out, bitpix, naxis, naxes, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to create image FITS file."; }

  num_unwritten_rows = dest_file->crop.height;
  delete[] naxes;
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
  if (num_unwritten_rows > 0) 
    { kdu_error e; e << "Not all rows were written to file."; }

  delete[] fpixel;

  fits_close_file(out, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to close FITS image!"; }

}

/*****************************************************************************/
/*                           fits_out::write_stripe                          */
/*****************************************************************************/

void
fits_out::write_stripe(int height, float* buf, ska_dest_file* const dest_file)
{
  int stripe_elements = dest_file->crop.width * height;
  // "buf" will be of size "stripe_elements * bytes_per_sample"
  std::cout << fpixel[0] << " " << fpixel[1] << std::endl;
  std::cout << height << " " << dest_file->crop.width << std::endl;

  fits_write_pixll(out, TFLOAT, fpixel, stripe_elements, buf, &status);
  if (status != 0)
    { kdu_error e; e << "FITS file terminated prematurely!"; }
  int anynul = 0;
  unsigned char nulval = 0;
  fits_read_pixll(out, TFLOAT, fpixel, stripe_elements, &nulval, buf, 
      &anynul, &status);
  std::cout << "STAT" << status << std::endl;

  // increment the position in FITS file
  fpixel[1] += height; 

  num_unwritten_rows -= height;
}
