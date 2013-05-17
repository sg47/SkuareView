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
#include "fits_local.h"
// fits includes
#include "fitsio.h"
// Skuareview includes
#include "sample_converter.h"

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
/*                                   fits_in                                 */
/* ========================================================================= */

/*****************************************************************************/
/*                               fits_in::fits_in                            */
/*****************************************************************************/

fits_in::fits_in(const char *fname, 
    kdu_args &args, 
    kdu_image_dims &dims, 
    int &next_comp_idx,
    bool &vflip,
    kdu_rgb8_palette *palette)
{
  incomplete_lines = NULL;
  free_lines = NULL;
  num_unread_rows = 0;
  fits.deafult_min = -50.0;
  fits.deafult_min = 50.0;
  //  initial_non_empty_tiles = 0;

  fits.startStoke = fits.startPlane = 1; // default (FITS frames and stokes are counted from 1)
  fits.endStoke = fits.endPlane = 0; 
  fits.transform = NONE;

  if(!parse_fits_parameters(args))
  { kdu_error e;
    e << "Error occured parsing FITS command line parameters";
  }

  // Open specified file for read only access.
  fits_open_file(&in, fname, READONLY, &status); 
  if (status != 0)
  { kdu_error e;
    e << "Unable to open input FITS file.";
  }

  fits_get_hdu_type(in, &cinfo.type, &status);
  if (status != 0)
  { kdu_error e;
    e << "Unable to get FITS type.";
  }

  fits_get_img_type(in, &cinfo.bitpix, &status);
  if (status != 0)
  { kdu_error e;
    e << "Unable to get FITS image type.";
  }

  // Get number of dimensions.
  fits_get_img_dim(in, &cinfo.naxis, &status);
  if (status != 0)
  { kdu_error e;
    e << "Unable to get dimensions of FITS image.";
  }
  if (cinfo.naxis<2)
  { kdu_error e;
    e << "FITS file does not contain image as it has less than 2 dimensions.";
  }

  // Get length of each dimension.
  int maxdim = sizeof(LONGLONG); //maximum dimentions to be returned
  LONGLONG *naxes = (LONGLONG *) malloc(sizeof(LONGLONG) * cinfo.naxis);   
  if (naxes == NULL)
  { kdu_error e;
    e << "Unable to allocate memory to get dimensions of FITS file.";
  }

  fits_get_img_sizell(in, maxdim, naxes, &status);   
  if (status != 0)
  { kdu_error e;
    free(naxes); 
    e << "Unable to get FITS image size.";
  }


  // Record image dimensions.
  cols = cinfo.width = naxes[0];
  rows = cinfo.height = naxes[1];
  std::cout << "FITS image's dimensions:\n"
    "cols = " << cols << "\n"
    "rows = " << rows << "\n";

  // Now see if cropping is required
  int crop_y, crop_x, crop_height, crop_width;
  if (dims.get_cropping(crop_y,crop_x,crop_height,crop_width,next_comp_idx))
  {
    if ((crop_x < 0) || (crop_y < 0))
    { kdu_error e; e << "Requested input file cropping parameters are "
      "illegal -- cannot have negative cropping offsets.";
    }
    if ((crop_x+crop_width) > cols)
    { kdu_error e; e << "Requested input file cropping parameters are "
      "not compatible with actual image dimensions.  The cropping "
        "region would cross the right hand boundary of the image.";
    }
    if ((crop_y+crop_height) > rows)
    { kdu_error e; e << "Requested input file cropping parameters are "
      "not compatible with actual image dimensions.  The cropping "
        "region would cross the lower hand boundary of the image.";
    }
    skip_cols = crop_x;   skip_rows = crop_y;
    rows = crop_height;   cols = crop_width;
  }
  else
  {
    skip_cols = skip_rows = 0;
  }

  // Check if we are dealing with a three (or greater) dimensional image.
  // We can deal with 2 (planar images), 3 (data cubes) or 4 (data cubes with
  // multiple stokes) dimensional image.  Sometimes, naxis is >4, but all 
  // the higher dimensions are 1.  In this case, we can interpret this as 
  // a 4 dimensional image. Check if that this is the case.
  if (cinfo.naxis > 2) {
    cinfo.depth = naxes[2];  // number of frames
    std::cout << "depth = " << cinfo.depth << "\n";
    if (cinfo.depth < planesout) {
      kdu_error e; 
      e << "The number available frames in FITS is less than requested!"; 
    }
    if (cinfo.naxis > 3) {
      cinfo.stokes = naxes[3];
      if (cinfo.stokes < tilesout) {
        kdu_error e; 
        e << "The number available stokes in FITS is less than requested!"; 
      }

      // Check higher dimensions are length one.
      for (int i=4; i<cinfo.naxis; i++) {
        if (naxes[i]>1)
        { kdu_error e;
          free(naxes);
          e << "Dimension " << i+1 << " has a length greater than 1.";
        }
      }
    }
  }

  free(naxes);

  if(cinfo.stokes > 1){
    if(fits.endStoke == 0){  // by default take everything
      fits.startStoke = 1;
      fits.endStoke = cinfo.stokes;
      tilesout = abs(fits.endStoke - fits.startStoke);
    }
  }else{
    fits.startStoke = fits.endStoke = 1; 
    tilesout = 1;
  }

  if (cinfo.depth > 1) {
    if(fits.endPlane == 0){
      fits.startPlane = 1;
      fits.endPlane = cinfo.depth;
      planesout = abs(fits.endPlane - fits.startPlane + 1);
    }
  }else{
    fits.startPlane = fits.endPlane = 1;
    planesout = 1;
  }

  num_components = planesout;

  // Prepare initial fpixel for CFITSIO. Will be used in fits_in::get
  fpixel = (LONGLONG *) malloc(sizeof(LONGLONG) * cinfo.naxis);

  // Left corner of the image
  fpixel[0] = skip_cols + 1;
  fpixel[1] = skip_rows + 1;
  if (cinfo.naxis>2) {
    fpixel[2] = current_frame = fits.startPlane;
    if (cinfo.naxis>3) {
      fpixel[3] = current_stoke = fits.startStoke;
      if (cinfo.naxis>3)
        for (int i=4; i<cinfo.naxis; i++) // For any dimension > 4, the width if always 1
          fpixel[i] = 1;
    }
  } 

  // Read header
  fits_get_hdrspace(in,&nkeys,NULL, &status);
  if (status != 0) 
  { kdu_error e;
    e << "Unable to get the number of header keywords in FITS file";
  }

  float_minvals = fits.deafult_min;
  float_maxvals = fits.deafult_max;

  for (int ii=1; ii<=nkeys; ii++) {
    fits_read_keyn(in,ii,keyname,keyvalue,keycomment, &status);
    if (status != 0)
    { kdu_error e;
      e << "Error reading keyword number " << ii;
    }

    // overwitte default min and max values for the entire image
    if (!fits.minmax)
    {
      if (!strcmp(keyname, "DATAMIN")) sscanf(keyvalue, "%lf", &float_minvals);
      if (!strcmp(keyname, "DATAMAX")) sscanf(keyvalue, "%lf", &float_maxvals);
    }

    if (fits.meta)
      std::cout << keyname << ": " << keyvalue << "\n  Comment: " << keycomment << "\n";

  }
  std::cout << "\nThe following values of MIN and MAX will be used:\n";
  std::cout << "DATAMIN = " << float_minvals << "\n";
  std::cout << "DATAMAX = " << float_maxvals << "\n";

  // scaling of FITS when reading
  {
    double scale = 1.0;
    double zero = 0.0;

    switch (fits.transform) { // todo: implement scaling
      case NONE:
        scale = 1.0;
        zero = 0.0;
        break;
      case LOG:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_LOG:
        scale = 1.0;
        zero = 0.0;
        break;
      case LINEAR:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_LINEAR:
        scale = 1.0;
        zero = 0.0;
        break;
      case RAW:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_RAW:
        scale = 1.0;
        zero = 0.0;
        break;
      case SQRT:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_SQRT:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_SQUARED:
        scale = 1.0;
        zero = 0.0;
        break;
      case POWER:
        scale = 1.0;
        zero = 0.0;
        break;
      case NEGATIVE_POWER:
        scale = 1.0;
        zero = 0.0;
        break;          
      default:
        break;
    }

    fits_set_bscale(in, scale, zero, &status); // Setting up the scaling
    if (status != 0)
    { kdu_error e;
      e << "Could not turn the scaling off!";
    }
  }

  // Set initial parameters -- these may change if there is a colour palette
  //   expand_palette = remap_samples = invert_first_component = false;

  switch (cinfo.bitpix) { 
    case BYTE_IMG:      
      sample_bytes = 1;
      bitspersample = 8;
      break;
    case SHORT_IMG:     
      sample_bytes = 2;
      bitspersample = 16;
      break;
    case LONG_IMG:      
      sample_bytes = 4;
      bitspersample = 32;
      break;
    case LONGLONG_IMG:  
      sample_bytes = 8;
      bitspersample = 64;
      break;
    case FLOAT_IMG:     
      sample_bytes = 4;
      bitspersample = 32;
      break;
    case DOUBLE_IMG:    
      sample_bytes = 8;
      bitspersample = 64;
      break;
    default:            
      kdu_error e; e << "The number of bits in FITS " 
        "image does not seem to be defined!";
      break;
  }

  int num_colours = 1;
  int colour_space_confidence = 0;
  jp2_colour_space colour_space = JP2_sLUM_SPACE; // monochomatic
  bool has_premultiplied_alpha=false;  //at this stage we declare "no alpha components"
  bool has_unassociated_alpha=false;

  if (next_comp_idx == 0)
    dims.set_colour_info(num_colours,
        has_premultiplied_alpha,
        has_unassociated_alpha,  
        colour_space_confidence,
        colour_space);

  first_comp_idx = next_comp_idx;

  // Very simple handling of -fprec parameter (probably too simplified)
  bool align_lsbs = false;
  int forced_prec = dims.get_forced_precision(first_comp_idx, align_lsbs);
  if (forced_prec > 0) {
    bitspersample = forced_prec;
  }


  bytesample = bitspersample/8;
  num_bytes = cols * bytesample;
  precision = bitspersample;

  if (cinfo.bitpix == BYTE_IMG){ 
    for (int n=0; n < num_components; n++)
    {
      dims.add_component(rows,cols,precision,false,next_comp_idx);
      next_comp_idx++;
    }
  } else{
    for (int n=0; n < num_components; n++)
    {
      dims.add_component(rows,cols,precision,true,next_comp_idx);
      next_comp_idx++;
    }            
  }

  num_unread_rows = rows * tilesout * planesout; //total number ot rows to read
  vflip = true;
}

/*****************************************************************************/
/*                               fits_in::~fits_in                           */
/*****************************************************************************/

fits_in::~fits_in()
{
  if ((num_unread_rows > 0) || (incomplete_lines != NULL))
  { kdu_warning w;
    w << "Not all rows of image component "
      << first_comp_idx << " were consumed!";
  }
  image_line_buf *tmp;
  while ((tmp=incomplete_lines) != NULL)
  { incomplete_lines = tmp->next; delete tmp;
  }
  while ((tmp=free_lines) != NULL)
  { free_lines = tmp->next; delete tmp;
  }
  fits_close_file(in, &status);
  if (status != 0)
  { kdu_error e; 
    e << "Unable to close FITS image!";
  }

}

/*****************************************************************************/
/*                                 fits_in::get                              */
/*****************************************************************************/

bool
fits_in::get(int comp_idx, // component number: 0 to num_components
    kdu_line_buf &line, 
    int x_tnum  // tile number, starts from 0. We use tiles for stokes
    )
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
          " is not supported yet";
        }
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
        break;
        }
      case DOUBLE_IMG:    
        {kdu_error e; e << "Double precition floating point"
          " is not supported yet";
        }
        break;
      default:            
        {kdu_error e; e << "The number of bits in FITS " 
          "image does not seem to be defined!";
        }
        break;
    }

    if (status != 0)
    { kdu_error e; e << "FITS file for component " << comp_idx
      << " terminated prematurely!";
    }

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
            "Example: -minmax {-1.0,1.0}";
        }
      }
      args.advance();
    }

    if (args.find("-iplane") != NULL) {
      for (int i = 0; i < 2; ++i) { 
        const char *string = args.advance();
        bool succ = true;
        for (int j = 0; j < strlen(string); ++j)
          if (!std::isdigit(string[j])) 
            succ = false;
        if (!succ || (i == 0 && 
              (sscanf(string, "%ld", &fits.startPlane) != 1 ||
               fits.startPlane < 0)))
          succ = false;
        else if (!succ || (i == 1 && 
              (sscanf(string, "%ld", &fits.endPlane) != 1 ||
               fits.endPlane < 0 ||
               fits.endPlane <= fits.startPlane)))
          succ = false;

        if (!succ)
        { kdu_error e; e << "\"-plane\" argument contains malformed "
          "plane specification. Expected to find two comma-seperated "
            "non-negative integers, enclosed by curly braces. The second "
            "must be larger than the first.";
        }
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
            "separated first and last Stok parameters to read.";
          }
          string++; // Walk past the separator
        }
        if (*string == '\0')
          break;
        if (((field_sep = strchr(string,'}')) != NULL) &&
            (*(++field_sep) == '\0'))
          field_sep = NULL;

        if ((sscanf(string,"{%ld,%ld}", &fits.startStoke,
                &fits.endStoke) != 2) ||
            (fits.startStoke < 1) || (fits.endStoke < 1) || 
            (fits.startStoke <= fits.endStoke) || 
            (fits.startStoke > 5) ||
            (fits.endStoke > 5))
        { kdu_error e; e << "\"-istok\" argument contains malformed "
          "stok specification.  Expected to find two comma-separated "
            "integers, enclosed by curly braces.  They must be strictly "
            "parameter must be larger than the first one";
        }
      }
      args.advance();
    }

    if (args.find("-scale") != NULL){ // What scaling should be performed on the raw FITS data? 
      if ((string = args.advance()) == NULL){ 
        kdu_error e; 
        e << "\"-scale\" argument requires the type of scaling!"; 
      }else{
        if (strcasecmp(string,"LOG") == 0) {
          fits.transform = LOG;
        }
        else if (strcasecmp(string,"NEGATIVE_LOG") == 0) {
          fits.transform = NEGATIVE_LOG;
        }
        else if (strcasecmp(string,"LINEAR") == 0) {
          fits.transform = LINEAR;
        }
        else if (strcasecmp(string,"NEGATIVE_LINEAR") == 0) {
          fits.transform = NEGATIVE_LINEAR;
        }
        else if (strcasecmp(string,"RAW") == 0) {
          fits.transform = RAW;
        }
        else if (strcasecmp(string,"NEGATIVE_RAW") == 0) {
          fits.transform = NEGATIVE_RAW;
        }
        else if (strcasecmp(string,"SQRT") == 0) {
          fits.transform = SQRT;
        }
        else if (strcasecmp(string,"NEGATIVE_SQRT") == 0) {
          fits.transform = NEGATIVE_SQRT;
        }
        else if (strcasecmp(string,"SQUARED") == 0) {
          fits.transform = SQUARED;
        }
        else if (strcasecmp(string,"NEGATIVE_SQUARED") == 0) {
          fits.transform = NEGATIVE_SQUARED;
        }
        else if (strcasecmp(string,"POWER") == 0) {
          fits.transform = POWER;
        }
        else if (strcasecmp(string,"NEGATIVE_POWER") == 0) {
          fits.transform = NEGATIVE_POWER;
        }
        else {
          kdu_error e;
          e << "Unknown transform specified:" << string << " Using default instead.\n";
        }
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
