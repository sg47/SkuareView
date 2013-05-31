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
fits_out::write_header(jp2_family_tgt &tgt, kdu_args &args, 
    ska_dest_file* const dest_file)
{
  kdu_error e;
  // Initialize state information in case we have to clean up prematurely
  dest_file->is_signed = NULL;
  dest_file->precision = NULL;
  dest_file->num_unwritten_rows = 0;

  /* Retrieve and use varaibles related to the input JPX image */

  bitpix = FLOAT_IMG;
  naxis = 2;

  // Create destination FITS file
  fits_create_file(&out, dest_file->fname, &status);

  long int* naxes = (long int*) malloc(sizeof(long int) * naxis);
  fits_create_img(out, bitpix, naxis, naxes, &status);
  dest_file->crop.width = naxes[0];
  dest_file->crop.height = naxes[1];
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
  kdu_warning w;
  kdu_error e;

  fits_close_file(out, &status);
  if (status != 0)
    e << "Unable to close FITS image!";
}

/*****************************************************************************/
/*                               fits_out::put                               */
/*****************************************************************************/

void
fits_out::write_stripe(int height, kdu_byte* buf, 
    ska_dest_file* const dest_file)
{
  kdu_error e;
  kdu_warning w;

  int anynul=0;
  unsigned char nulval=0;

  switch (bitpix) { 
    case FLOAT_IMG:  
      {
        //fits_write_pixll(out, TFLOAT, fpixel, dest_file->crop.width, 
            //&nulval, buf, &anynul, &status);
        free(buf);
        break;
      }
  }

  if (status != 0)
    e << "FITS file terminated prematurely!"; 

  // increment the position in FITS file
  fpixel[0] = dest_file->crop.x + 1; // read from the begining of line
  fpixel[1] += dest_file->crop.height; // keep the frame and stoke the same and increment the row

  dest_file->num_unwritten_rows -= height;
}
