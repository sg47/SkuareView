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

enum domain { LOG, SQRT, LINEAR };

/*****************************************************************************/
/* STATIC                   irreversible_renormalize                         */
/*****************************************************************************/

static void
irreversible_renormalize(float *buf, int buf_length, 
    double minval, double maxval, domain samples_domain)
{
  float scale, factor, offset=0.0;

  /* The images captured in radio astronomy have extremely dynamic range of
   * values. As such a linear scaling will often result an over compressed 
   * image, because most of the data will end up being extremely close 
   * together. As such we use a log or sqrt domain to minimize the loss in 
   * precision */
  if (samples_domain == LINEAR) {
    scale = fabs(maxval-minval);
  }
  else if (samples_domain == LOG) { // invert the log transform
    factor = 500;
    scale = log((maxval - minval) * factor + 1);
  }
  else if (samples_domain == SQRT) { // invert the sqrt transform
    scale = sqrt(fabs(maxval-minval));
  }
  offset = minval;   

  for (int i = 0; i < buf_length; i++)
  {
    float fval = (double)((buf[i] + 0.5) * scale);
    if (samples_domain == LINEAR) 
      fval += offset;    
    else if (samples_domain == LOG) 
      fval = (exp(fval) - 1) / factor + offset;
    else if (samples_domain == SQRT) 
      fval = fval * fval + offset; 

    fval = (fval > minval)?fval:minval;
    fval = (fval < maxval)?fval:maxval;
    buf[i]= fval;
  }
}

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
  //TODO: make dynamic (these parmaters currently only work 32 bit floating
  //point samples)
  dest_file->is_signed = true;
  dest_file->precision = 32;
  dest_file->reversible = false;
  dest_file->bytes_per_sample = 4;
  bitpix = FLOAT_IMG;
  naxis = 3; 
  num_unwritten_rows = 0;
  status = 0;

  fpixel = new LONGLONG [naxis];
  fpixel[0] = dest_file->crop.x+1;
  fpixel[1] = dest_file->crop.y+1;
  fpixel[2] = dest_file->crop.z+1;
  for(int i = 3; i < naxis; ++i)
    fpixel[i] = 1;
  /* Retrieve and use varaibles related to the input JPX image */

  long* naxes = new long [naxis];
  naxes[0] = dest_file->crop.width;
  naxes[1] = dest_file->crop.height;
  naxes[2] = dest_file->crop.depth;
  for(int i = 3; i < naxis; ++i)
    naxes[i] = 1;
  std::cout << "Decoding JPX image with dimensions:" << std::endl;
  std::cout << naxes[0] << " " << naxes[1] << " " << naxes[2] << std::endl;

  // fits file names must be preceded by a '!' in order to overwrite files in
  // cfitsio
  char* fitsfname = new char [BUFSIZ];
  fitsfname[0] = '!';
  for(int i = 0; dest_file->fname[i] != '\0'; ++i) 
    fitsfname[i+1] = dest_file->fname[i];
  dest_file->fname = fitsfname;

  // Create destination FITS file
  fits_create_file(&out, dest_file->fname, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to create FITS file."; }

  int iomode = READWRITE;
  fits_file_mode(out, &iomode, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to write to FITS file."; }

  fits_create_img(out, bitpix, naxis, naxes, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to create image FITS file."; }

  num_unwritten_rows = dest_file->crop.height * dest_file->crop.depth;
  delete[] naxes;

  // Open FITS Header information box and write that to the FITS file
  jp2_input_box meta_box;
  meta_box.open(&src); //Open the main jp2 box
  meta_box.close();
  meta_box.open_next(); //Open the first subbox
  while(meta_box.exists() && !(meta_box.get_box_type() == 75756964) ) {
    meta_box.close();
    meta_box.open_next();
  }
  if (meta_box.exists()) {

    kdu_byte ska_uuid[16] = {0x24,0x37,0xE6,0xC0,
                            0xF2,0xB2,0x11,0xE2,
                            0xB7,0x78,0x08,0x00,
                            0x20,0x0C,0x9A,0x66};

    kdu_byte* metadata_buf = new kdu_byte [BUFSIZ];
    int contents_length = meta_box.get_box_bytes();
    meta_box.read(metadata_buf, meta_box.get_box_bytes());
    // check uuid
    bool correct_uuid = true;
    for(int i=0; i<16; ++i) {
      if (ska_uuid[i] != metadata_buf[i]) {
        correct_uuid = false;
        break;
      }
    }
    if (correct_uuid) {
      char* record = new char [80]; // max length of a FITS record
      int record_idx = 0;
      int nkeys = 0;
      fits_get_hdrspace(out, &nkeys, NULL, &status);
      int prev_keys = nkeys;
      for (int i=16; i<contents_length; ++i) {
        if (metadata_buf[i] == '?') {
          fits_write_record(out, record, &status);
          if (status != 0)
            { kdu_error e; e << "Unable to write record " << record; }
          record_idx = 0;
          nkeys++;
        }
        else {
          record[record_idx++] = metadata_buf[i];
          // set new end of record (also clears out previous records
          record[record_idx] = '\0';
        }
      }
      for (int i=1; i<nkeys-prev_keys; i++) {
        // read for data min and max value
        fits_read_keyn(out,i,keyname,keyvalue,keycomment, &status);
          if (status != 0)
            { kdu_error e; e << "Unable to write record " << record; }

        // overwrite default min and max values for the entire image
        if (!strcmp(keyname, "DATAMIN")) 
          sscanf(keyvalue, "%lf", &dest_file->samples_min);
        if (!strcmp(keyname, "DATAMAX")) 
          sscanf(keyvalue, "%lf", &dest_file->samples_max);
        char del_key[4][10] = {"NAXIS1", "NAXIS2", "NAXIS3", "NAXIS"};
        for(int j = 0; j < 4; ++j)
          if (!strcmp(keyname, del_key[j]))  {
             fits_delete_key (out, del_key[j], &status);
          }
      }
    } else {
      fits_write_key(out, TFLOAT, "DATAMIN", &(dest_file->samples_min), NULL, 
          &status);
      fits_write_key(out, TFLOAT, "DATAMAX", &(dest_file->samples_max), NULL, 
          &status);
      if (status != 0)
      { kdu_error e; e << "Unable to write min/max keywords to FITS file."; }
    }

  }
  meta_box.close();

  std::cout << "\nThe following values of MIN and MAX will be used:\n";
  std::cout << "DATAMIN = " << dest_file->samples_min << "\n";
  std::cout << "DATAMAX = " << dest_file->samples_max << "\n";

  frame_fheight = new long [dest_file->crop.depth];
  for (int i = 0; i < dest_file->crop.depth; ++i)
    frame_fheight[i] = 1;
}

/*****************************************************************************/
/*                            fits_out::~fits_out                            */
/*****************************************************************************/

fits_out::~fits_out()
{
  if (num_unwritten_rows > 0) 
    { kdu_error e; e << "Not all rows were written to file."; }

  delete[] fpixel;
  delete[] frame_fheight;

  fits_close_file(out, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to close FITS image!"; }
}

/*****************************************************************************/
/*                           fits_out::write_stripe                          */
/*****************************************************************************/

void
fits_out::write_stripe(int height, float* buf, ska_dest_file* const dest_file,
    int component)
{
  int stripe_elements = dest_file->crop.width * height;
  // "buf" will be of size "stripe_elements * bytes_per_sample"
  irreversible_renormalize(buf, stripe_elements, dest_file->samples_min,
      dest_file->samples_max, LINEAR);

  stripe_elements = dest_file->crop.width;
  fpixel[0] = dest_file->crop.x + 1; // read from the begining of line
  fpixel[1] = frame_fheight[component];
  fpixel[2] = component+1;

  
  for(int i = 0; i < height && fpixel[1] <= dest_file->crop.height; 
      i++, buf+=dest_file->crop.width, fpixel[1]++) {
    fits_write_pixll(out, TFLOAT, fpixel, stripe_elements, buf, &status);
    if (status != 0)
      { kdu_error e; e << "FITS file terminated prematurely!"; }
  }
  buf -= dest_file->crop.width * height;

  num_unwritten_rows -= height;
  frame_fheight[component] += height;
}
