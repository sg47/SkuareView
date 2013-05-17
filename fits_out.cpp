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

    // Create destination FITS file
    fits_create_file(&out, fname, &status);

    int* naxes = (int*) malloc(sizeof(int) * cinfo.naxis);
    fits_create_img(&out, cinfo.bitpix, cinfo.naxis, naxes, &status);
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
  kdu_warning w;
  kdu_error e;

  if (num_unwritten_rows > 0 || incomplete_lines != NULL)
    w << "Not all rows of the J2K image were consumed!";

  fits_close_file(out, &status);
  if (status != 0)
    e << "Unable to close FITS image!";
}

/*****************************************************************************/
/*                               fits_out::put                               */
/*****************************************************************************/

void
fits_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
  int width = line.get_width();
  float *buffer =(float*) malloc(sizeof(float)*width);
  int idx = comp_idx;
  assert((idx >= 0) && (idx < num_components));

  image_line_buf *scan, *prev=NULL;
  for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next)
  {
    assert(scan->next_x_tnum >= x_tnum);
    if (scan->next_x_tnum == x_tnum)
      break;
  }
  if (scan == NULL)
  { // Need to read a new image line.
    assert(x_tnum == 0); // Must consume line from left to right.
    if (num_unread_rows == 0)
      return false;

    if (scan == NULL)
    { // Need to read a new image line.
      assert(x_tnum == 0); // Must consume in very specific order.
      if (num_unread_rows == 0)
        return false;

      if ((scan = free_lines) == NULL)
        scan = new image_line_buf(width, bytesample);
      // Big enough for padding and expanding bits to bytes
      free_lines = scan->next;
      if (prev == NULL)
        incomplete_lines = scan;
      else
        prev->next = scan;
      scan->accessed_samples = 0;
      scan->next_x_tnum = 0;       
    }

    int anynul=0;
    unsigned char nulval=0;

    switch (cinfo.bitpix) { 
      case BYTE_IMG:      
        fits_read_pixll(in, TBYTE, fpixel, num_bytes, &nulval, 
            scan->buf, &anynul, &status);
        convert_words_to_shorts(scan->buf,
            line.get_buf16(),width,
            precision,false,sample_bytes,
            littlendian);
        break;
      case SHORT_IMG:     
        fits_read_pixll(in, TSHORT, fpixel, width, &nulval, 
            scan->buf, &anynul, &status);
        convert_words_to_shorts(scan->buf,
            line.get_buf16(),width,
            precision,true,sample_bytes,
            littlendian);
        break;
      case LONG_IMG:      
        fits_read_pixll(in, TLONG, fpixel, width, &nulval, 
            scan->buf, &anynul, &status);
        convert_words_to_ints(scan->buf,
            line.get_buf32(),width,
            precision,true,sample_bytes,
            littlendian);
        break;
      case LONGLONG_IMG:  
        {kdu_error e; e << "LONGLONG precition"
          " is not supported yet";}
          break;
      case FLOAT_IMG:     //<##>
          {fits_read_pixll(in, TFLOAT, fpixel, width, &nulval, 
              buffer, &anynul, &status);
          kdu_sample32 *buf32 = line.get_buf32();
          // longer works correctly. Maybe a thread issue?
          if (line.is_absolute()) { // reversible transformation
            convert_TFLOAT_to_ints(buffer, buf32, width,
                precision, true, float_minvals,
                float_maxvals, sample_bytes);
          }
          else {
            convert_TFLOAT_to_floats(buffer, buf32, width, 
                true, float_minvals, float_maxvals);
          }

          free(buffer);
          break;}
      case DOUBLE_IMG:    
          {kdu_error e; e << "Double precition floating point"
            " is not supported yet";}
            break;
      default:            
            {kdu_error e; e << "The number of bits in FITS " 
              "image does not seem to be defined!";}
              break;
    }

    if (status != 0)
    { kdu_error e; e << "FITS file for component " << comp_idx
      << " terminated prematurely!"; }

      // increment the position in FITS file
      fpixel[0] = skip_cols + 1; // read from the begining of line
      if(fpixel[1]==rows){ // if reached the end of what to be read
        fpixel[1] = skip_rows + 1;  // set to the begining for the next frame
        if (cinfo.naxis>2 && cinfo.depth >1){
          if (current_frame!=fits.endPlane){
            fpixel[2] = ++current_frame; // next frame
            ++comp_idx;         //new frame - new next time component
          }
          else if (cinfo.naxis>3 && cinfo.stokes>1){
            fpixel[2] = fits.startPlane;
            fpixel[3] = ++current_stoke; // next stoke
            scan->next_x_tnum++; //thext stoke -> next tile
          }
        }                                                                   
      }
      else 
        fpixel[1]++; // keep the frame and stoke the same and increment the row

      num_unread_rows--;
  }

  assert((scan->width - scan->accessed_samples) >= width);
  scan->accessed_samples += scan->width; 
  if (scan->accessed_samples == scan->width)
  { 
    assert(scan == incomplete_lines);
    incomplete_lines = scan->next;
    scan->next = free_lines;
    free_lines = scan;
  }

  return true;
}


