/*****************************************************************************/
//
//  @file: fits_in.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Slava Kitaeff
//  @date 29/07/12.
//  @brief Implements file reading from the FITS file format, provides support
//         up to 4 dimensions and is readily extendible to include further
//         features.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/******************************************************************************/

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
#include "kdu_args.h"
// Image includes
#include "kdu_image.h"
// IO includes
#include "kdu_file_io.h"
#include "image_local.h"
// FITS includes
#include "fitsio.h"
#include "fits_local.h"
#include "sample_converter.h"

/*****************************************************************************/
/* STATIC                   irreversible_normalize                           */
/*****************************************************************************/

static void
  irreversible_normalize(float *buf, int buf_length, 
    bool is_signed, double minval, double maxval, bool cerr_samples)
{
  //TODO: add normalization domain
  
  float factor, scale, offset=0.0;
  float limmin=-0.75, limmax=0.75;
  offset = -0.5;
  scale = 1.0 / fabs(maxval-minval);

  for (int i = 0; i < buf_length; i++)
  {
    float fval = (buf[i]-minval) * scale + offset;
    fval = (fval > limmin)?fval:limmin;
    fval = (fval < limmax)?fval:limmax;
    buf[i] = fval;
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
/*                                   fits_in                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                           read_header::fits_in                            */
/*****************************************************************************/

void
fits_in::read_header(jp2_family_tgt &tgt, kdu_args &args, 
    ska_source_file* const source_file)
{
  // In case we terminate early
  num_unread_rows = 0;
  source_file->is_signed = true;

  if(!parse_fits_parameters(args))
    { kdu_error e; e << "Error occured parsing FITS command line parameters"; }

  // Open specified file for read only access.
  fits_open_file(&in, source_file->fname, READONLY, &status); 
  if (status != 0)
    { kdu_error e; e << "Unable to open input FITS file."; }

  bool valid_hdu_found = true;
  do {
    valid_hdu_found = true;
    fits_get_img_type(in, &bitpix, &status);
    if (status != 0)
      { kdu_error e; e << "Unable to get FITS image type."; }

    // Get number of dimensions.
    fits_get_img_dim(in, &naxis, &status);
    if (status != 0)
      { kdu_error e; e << "Unable to get dimensions of FITS image."; }
    if (naxis < 2) {
      fits_movrel_hdu(in, 1, NULL, &status);  /* try to move to next HDU */
      if (status != 0)
        { kdu_error e; e << "No valid hdu found in FITS image."; }
      valid_hdu_found = false;
    }
  } while (!valid_hdu_found);

  // Get length of each dimension.
  int maxdim = sizeof(LONGLONG); // maximum dimentions to be returned
  LONGLONG *naxes = (LONGLONG *) malloc(sizeof(LONGLONG) * naxis);   
  if (naxes == NULL)
    { kdu_error e; e << "Unable to allocate memory to get dimensions of FITS file."; }

  fits_get_img_sizell(in, maxdim, naxes, &status);   
  if (status != 0) { 
    free(naxes); 
    kdu_error e; e << "Unable to get FITS image size.";
  }
  std::cout << "FITS dimensions [" << naxis << "]: ";
  for(int i = 0; i < naxis; i++) 
    std::cout << naxes[i] << " ";
  std::cout << std::endl;
  
  // Prepare initial fpixel for CFITSIO. Will be used in fits_in::get
  fpixel = (LONGLONG*) malloc(sizeof(LONGLONG) * naxis);

  // Left corner of the image
  // The FITS library iterates from 1 instead of from 0
  if (source_file->crop.specified) {
    fpixel[0] = source_file->crop.x + 1;
    fpixel[1] = source_file->crop.y + 1;
    if (naxis > 2)
      fpixel[2] = source_file->crop.z + 1;
  }
  else {
    fpixel[0] = 1;
    fpixel[1] = 1;
    if (naxis > 2)
      fpixel[2] = 1;
    source_file->crop.width = naxes[0];
    source_file->crop.height = naxes[1];
    source_file->crop.depth = naxes[2];
  }
  if (naxis > 3)
    fpixel[3] = 1;
  free(naxes);

  double scale = 1.0;
  double zero = 0.0;

  fits_set_bscale(in, scale, zero, &status); // Setting up the scaling
  if (status != 0)
    { kdu_error e; e << "Could not turn the scaling off!"; }

  // Set initial parameters -- these may change if there is a colour palette
  //   expand_palette = remap_samples = invert_first_component = false;

  switch (bitpix) { 
    case BYTE_IMG:      
      source_file->bytes_per_sample = 1;
      source_file->precision = 8;
      break;
    case SHORT_IMG:     
      source_file->bytes_per_sample = 2;
      source_file->precision = 16;
      break;
    case LONG_IMG:      
      source_file->bytes_per_sample = 4;
      source_file->precision = 32;
      break;
    case LONGLONG_IMG:  
      source_file->bytes_per_sample = 8;
      source_file->precision = 64;
      break;
    case FLOAT_IMG:     
      source_file->bytes_per_sample = 4;
      source_file->precision = 32;
      break;
    case DOUBLE_IMG:    
      source_file->bytes_per_sample = 8;
      source_file->precision = 64;
      break;
    default:            
      kdu_error e; e << "The number of bits in FITS image does not seem to be defined!";
      break;
  }

  int num_colours = 1;
  int colour_space_confidence = 0;
  jp2_colour_space colour_space = JP2_sLUM_SPACE; // monochomatic
  bool has_premultiplied_alpha=false;  //at this stage we declare "no alpha components"
  bool has_unassociated_alpha=false;

  bool align_lsbs = false;
  if (source_file->forced_prec > 0) 
    source_file->precision = source_file->forced_prec;

  source_file->bytes_per_sample = source_file->precision / 8;

  //total number ot rows to read
  num_unread_rows = source_file->crop.height * 
    source_file->crop.depth;

  // Read header
  fits_get_hdrspace(in, &nkeys, NULL, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to get the number of keywords in FITS file."; }

  source_file->metadata_buffer = new kdu_byte [BUFSIZ];
  int buf_idx = 0; 
  for (int i=1; i<=nkeys; i++) {
    // read for data min and max value
    fits_read_keyn(in,i,keyname,keyvalue,keycomment, &status);
    if (status != 0)
    { kdu_error e; e << "Error reading keyword number " << i; }
    
    // overwrite default min and max values for the entire image
    if (!fits.minmax) {
      if (!strcmp(keyname, "DATAMIN")) sscanf(keyvalue, "%lf", &source_file->float_minvals);
      if (!strcmp(keyname, "DATAMAX")) sscanf(keyvalue, "%lf", &source_file->float_maxvals);
    }

    // put all the header data into metadata (in case we ever convert back to
    // FITS
    fits_read_record(in, i, record, &status);
    if (status != 0)
      { kdu_error e; e << "Error reading keyword number " << i; }
    // Then write to metadata
    for (int j=0; record[j] != '\0'; ++j) 
      source_file->metadata_buffer[buf_idx++] = record[j];
    source_file->metadata_buffer[buf_idx++] = '?'; // My character to split records
  }
  source_file->metadata_length = buf_idx;
  source_file->metadata_buffer[buf_idx--] = '\0';

  std::cout << "\nThe following values of MIN and MAX will be used:\n";
  std::cout << "DATAMIN = " << source_file->float_minvals << "\n";
  std::cout << "DATAMAX = " << source_file->float_maxvals << "\n";

  frame_fheight = new long [source_file->crop.depth];
  for(int i = 0; i < source_file->crop.depth; ++i)
    frame_fheight[i] = 0;
}

/*****************************************************************************/
/*                               fits_in::~fits_in                           */
/*****************************************************************************/

fits_in::~fits_in()
{
  if (num_unread_rows > 0)
    { kdu_warning w; w << "Not all rows were read!"; }
  fits_close_file(in, &status);
  if (status != 0)
    { kdu_error e; e << "Unable to close FITS image!"; }
  delete[] frame_fheight;
}

/*****************************************************************************/
/*                            fits_in::read_stripe                           */
/*****************************************************************************/

void
fits_in::read_stripe(int height, float *buf, ska_source_file* const source_file,
    int component)
{
  int anynul = 0;
  unsigned char nulval = 0;
  LONGLONG stripe_elements = source_file->crop.width;
  fpixel[0] = source_file->crop.x + 1; // read from the begining of line
  fpixel[1] = frame_fheight[component]+1;
  fpixel[2] = component+1;

  for(int i = 0; i < height; i++, buf+=source_file->crop.width, fpixel[1]++) {
    switch (bitpix) { 
      case FLOAT_IMG: 
        fits_read_pixll(in, TFLOAT, fpixel, stripe_elements, &nulval, buf, 
            &anynul, &status);
        break;
    }
    if (status != 0)
      { kdu_error e; e << "FITS file terminated prematurely!"; }
  }
  buf -= source_file->crop.width * height;

  // for undefined pixels the buffer is being filled with -nan,
  // with my current compiler isnan(-nan) is returning false.
  // I identified -nan to be 4294967295 in binary, so thats how i'm 
  // testing for this case...
  // TODO: show this horrible snippet of code to someone for better ideas
  union ufloat {
    float f;
    unsigned b;
  } uf; 
  uf.f = buf[0];
  // set undefined pixels to the minimum float value in the image
  if (uf.b == 4294967295)
    buf[0] = source_file->float_minvals;

  irreversible_normalize(buf, stripe_elements*height, source_file->is_signed, 
      source_file->float_minvals, source_file->float_maxvals, false);


  // increment the position in FITS file
  frame_fheight[component] += height;
  num_unread_rows -= height;
}

/*****************************************************************************/
/*                  fits_in::parse_fits_parameters                           */
/*                     FITS command line parser                              */
/*****************************************************************************/

bool fits_in::parse_fits_parameters(kdu_args &args)
{
  const char *string;

  if (args.get_first() != NULL) {

    if (args.find("-l") != NULL){ // should a lossless version be written
      fits.writeUncompressed = true;
      args.advance();
    }
    if (args.find("-1") != NULL){ // Gaussian noise standard deviation to add to image
      if ((string = args.advance()) == NULL){ 
        kdu_error e; 
        e << "\"-1\" argument requires standard diviation to add noise to the image!"; 
      }else{
        fits.noiseDB = atof(string);
        fits.noiseSet = true;
      }
      args.advance();
    }
    if (args.find("-2") != NULL){ // Gaussian noise (percentage) to add to raw FITS values
      if ((string = args.advance()) == NULL){ 
        kdu_error e; 
        e << "\"-2\" argument requires percentage to add noise to the image!"; 
      }else{
        fits.noisePct = atof(string);
      }
      args.advance();
    }
    if (args.find("-3") != NULL){ // Random number seed to generate Gaussian noise to add to image
      if ((string = args.advance()) == NULL){
        kdu_error e; 
        e << "\"-3\" argument requires a random number seed to generate noise!";
      }else{
        fits.seed = strtoul(string,NULL,10);

        if (errno != EINVAL){ // Check if conversion was successful.
          fits.seedSet = true;
        }else{  
          kdu_error e; 
          e << "\"-3\" argument is not a number!";
        }
      }
      args.advance();
    }
    if (args.find("-4") != NULL){ // Should all quality benchmark tests be performed?
      fits.writeNoiseField = true;
      args.advance();
    }
    if (args.find("-K") != NULL){ // should a lossless version be written
      // Main benchmarks
      fits.benchmarkQualityParameters.fidelity = true;
      fits.benchmarkQualityParameters.maximumAbsoluteDistortion = true;
      fits.benchmarkQualityParameters.meanAbsoluteError = true;
      fits.benchmarkQualityParameters.meanSquaredError = true;
      fits.benchmarkQualityParameters.peakSignalToNoiseRatio = true;
      fits.benchmarkQualityParameters.rootMeanSquaredError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;

      // Intermediate values
      fits.benchmarkQualityParameters.squaredError = true;
      fits.benchmarkQualityParameters.absoluteError = true;
      fits.benchmarkQualityParameters.squaredIntensitySum = true;
      args.advance();
    }
    if (args.find("-N") != NULL){ /* Should all quality benchmark tests be 
                                   * performed - but no intermediate results 
                                   * shown? This is the same as the above 
                                   * case, but without the intermediate 
                                   * values printed.
                                   */
      fits.benchmarkQualityParameters.fidelity = true;
      fits.benchmarkQualityParameters.maximumAbsoluteDistortion = true;
      fits.benchmarkQualityParameters.meanAbsoluteError = true;
      fits.benchmarkQualityParameters.meanSquaredError = true;
      fits.benchmarkQualityParameters.peakSignalToNoiseRatio = true;
      fits.benchmarkQualityParameters.rootMeanSquaredError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-B") != NULL){ // Fidelity benchmarking
      fits.benchmarkQualityParameters.fidelity = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-D") != NULL){ // PSNR benchmarking
      fits.benchmarkQualityParameters.peakSignalToNoiseRatio = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-G") != NULL){ // Maximum absolute distortion
      fits.benchmarkQualityParameters.maximumAbsoluteDistortion = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-H") != NULL){ // Mean squared error
      fits.benchmarkQualityParameters.meanSquaredError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-L") != NULL){ // Root mean squared error
      fits.benchmarkQualityParameters.rootMeanSquaredError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-U") != NULL){ // Mean absolute error
      fits.benchmarkQualityParameters.meanAbsoluteError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-V") != NULL){ // Squared error
      fits.benchmarkQualityParameters.squaredError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-Y") != NULL){ // Absolute error
      fits.benchmarkQualityParameters.absoluteError = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-X") != NULL){ // Squared uncompressed image intensity sum
      fits.benchmarkQualityParameters.squaredIntensitySum = true;
      fits.benchmarkQualityParameters.performQualityBenchmarking = true;
      args.advance();
    }
    if (args.find("-Z") != NULL){ // Should a residual image be written
      fits.benchmarkQualityParameters.writeResidual = true;
      args.advance();
    }
    if (args.find("-g") != NULL){ // Should compression benchmarking be performed
      fits.performCompressionBenchmarking = true;
      args.advance();
    }
    if (args.find("-meta") != NULL){ // Should input image metadata be displayed
      fits.meta = true;
      args.advance();
    }

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
        if (!succ || (i == 0 && (sscanf(string, "%f", &fits.deafult_min) != 1)))
          succ = false;
        else if (!succ || (i == 1 && 
              (sscanf(string, "%f", &fits.deafult_max) != 1)))
          succ = false;

        if (!succ)
        { kdu_error e; e << "\"-minmax\" argument contains "
          "malformed specification. Expected to find two comma-"
            "separated float numbers, enclosed by curly braces. "
            "Example: -minmax {-1.0,1.0}"; }
      }
      args.advance();
    }

  }

  /*
   * Compression benchmarking is only accurate if all planes of a data cube are read.  Display a message
   * informing the user of this if they specify a custom starting plane.
   */
  if (fits.performCompressionBenchmarking && (fits.startPlane != -1  || fits.startStoke != -1) ) {
    kdu_error e;
    e << "Compression benchmarking results are only accurate if all planes and stokes of a data cube\n"
      "or data volume are converted.  Beware of this when interpreting results.\n";
  }

  if (fits.seedSet && !fits.noiseSet) {
    kdu_error e;
    e << "A random number seed was set but not a target PSNR for noise addition.\n"
      "The seed will therefore be ignored.\n";
  }

  /*
   * Note if we were asked to write a noise field but not actually asked to add any noise.
   */
  if (fits.writeNoiseField && !fits.noiseSet) {
    kdu_error e;
    e << "Will not write noise field as no noise will be added to image.\n";
    fits.writeNoiseField = false;
  }

  return TRUE;

}
