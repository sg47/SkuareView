/*****************************************************************************/
//  
//  @file: fits_local.h
//  Project: Skuareview-NGAS-plugin
//
//  @author Slava Kitaeff
//  @date 29/07/12.
//  @brief The The file contains the definitions of types and classes
//  @brief for the FITS image format.
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
/*****************************************************************************/

#include <stdio.h> // C I/O functions can be quite a bit faster than C++ ones
#include "kdu_elementary.h"
#include "kdu_file_io.h"
#include "kdu_args.h"
#include "fitsio.h"
#include "ska_local.h"

/**
 * Structure allowing parameters for quality benchmarking to be specified
 * by the user.  Currently, numerous different quality benchmarks can be
 * specified by the user at the command line and information on these is
 * recorded in here.
 *
 * The last three metrics are not really of interest in their own right, but
 * are integer intermediate building blocks of some of the other metrics that
 * might be useful if integer, rather than floating point, raw data is desired.
 *
 * The last field specifies whether a residual iamge should be written to a
 * file.  This can be used even if no other quality benchmarks are specified.
 */

typedef struct {
  bool meanSquaredError; // Mean squared error
  bool rootMeanSquaredError; // Root mean squared error
  bool peakSignalToNoiseRatio; // Peak signal to noise ratio
  bool meanAbsoluteError; // Mean absolute error
  bool fidelity; // Fidelity
  bool maximumAbsoluteDistortion; // Maximum absolute distortion
  bool squaredError; // Squared error
  bool absoluteError; // Absolute error
  bool squaredIntensitySum; // Sum of squared uncompressed image intensities    
  bool performQualityBenchmarking; // Is at least one quality benchmark 
  // selected?  Intended to provide a quick check.  
  // Client code must keep this up to date
  bool writeResidual; // Should the residual image be written to a file?
} quality_benchmark_info;


/**
 * Enumerated type defining the transformations that may be performed
 * on raw FITS data to convert each datum into a 16 bit grayscale
 * (integer) intensity.
 *
 * Not all transforms will be defined for all FITS image types.
 */
typedef enum {
  DEFAULT // Default transform to use if no transform is explicitly specified.  
    // This will depend on the FITS data type.
} Transform;


typedef struct {
  Transform transform; // the transform to be performed on the raw data.  
  // This will be updated if a transform is specified on the 
  // command line using the parameter A.
  bool writeUncompressed; // specifying if a lossless version of image
  // should be written.  This will be set to true if 
  // the LL parameter is present on the command line.
  long startPlane;// First frame of data cube to read.  Ignored for 2D images.  
  // Will only be modified if the x parameter is present.  
  // If only x is specified (and not y), the single x
  //value will be interpreted as a single frame to read.
  long endPlane;  // Last frame of data cube to read.  Ignored for 2D images.  
  // Will only be modified if the y parameter is present.
  quality_benchmark_info benchmarkQualityParameters; // specifies what,
  // if any quality benchmark tests to be performed.  Assumed 
  // to be initialised to 'no benchmarks' before used.  
  // If the QB command line option is present, all benchmarks 
  // will be performed.  This will override any other quality 
  // parameters.  In the absence of this parameter, individual 
  // benchmarks may be turned on by specifying QB_FID for 
  // fidelity, QB_PSNR for peak signal to noise ratio, QB_MAD 
  // for maximum absolute distortion, QB_MSE for mean square 
  // error, QB_RMSE for root mean square error, QB_MAE for 
  // mean absolute error, QB_SE for squared error, QB_AE for 
  // absolute error and QB_SI for sum of uncompressed squared 
  // image intensities.  QB_RES specifies if a residual
  // image should be written.
  bool performCompressionBenchmarking; // specifying if compression benchmarking
  //should be performed on the images being compressed.  This 
  // will be set to true if the CB parameter is present on the 
  // command line.
  long startStoke; // startStoke First stoke of data volume to read.  Ignored 
  // for 2D or 3D images.  Will only be modified if the S1 
  // parameter is present.  If only S1 is specified (and not 
  // S2), the single S1 value will be interpreted as a single 
  // stoke to read.
  long endStoke; // Last stoke of data volume to read.  Ignored for 2D or 3D 
  // images.  Will only be modified if the S2 parameter is 
  // present.
  double noiseDB; // specifying the PSNR of the image after (Gaussian noise) 
  // has been added. Will not be changed unless the -noise 
  // command line parameter is present.
  bool noiseSet;  // specifying if the noiseDB parameter has been set by the user.  
  // Assumed to have been initialised to false.  Will be set 
  // to true if the -noise command line parameter is present.
  unsigned long seed; // Seed for the random number generator used to 
  // generate the noise specified by noiseDB.  Will be
  // ignored if the -noise command line parameter is not 
  // present.  If -noise is present but no seed is specified
  // the RNG will be seeded with the system clock time.  
  // Will be altered if the -seed command line parameter is
  //present.
  bool seedSet;   // seedSet Boolean specifying whether or not the -seed 
  // parameter is present.
  double noisePct;// noisePct Reference to a double specifying the percentage 
  // (of the difference between the minimum and maximum raw 
  // FITS values) standard deviation of Gaussian noise (with 
  // mean 0.0) to be added to raw FITS values (before transforming 
  // them into pixel intensities). Will not be changed unless 
  // the -noise_pct command line parameter is present.
  bool writeNoiseField; //specifying whether or not the -noise_field parameter 
  // is present, which determines whether or not the noise 
  // field should be written to a file.  Assumed to be 
  // initialised to false before this function is called and 
  // will not be changed if -noise_field and -noise are not 
  // present.
  bool meta; // -meta in command line specifies whether FITS header
  //is to be dysplayed or not
  bool  minmax; // -minmax in command line specifies minimum and maximum values
  // in input FITS image. These values are given for the entire FITS regardless
  // the number of image planes and stock parameters
  double deafult_min;  // if DATAMIN parameter is not present in FITS header default_min
  // will be used insted. Deafult_min value must be initialized in constructor. If minmax
  // is present in the command line arguments, deafult_min will be orevwritten with provided
  // value.
  double deafult_max;
  // if DATAMAX parameter is not present in FITS header default_min
  // will be used insted. Deafult_min value must be initialized in constructor. If minmax
  // is present in the command line arguments, deafult_max will be orevwritten with provided
  // value

} fits_param;

class fits_in;
class fits_out;

/*****************************************************************************/
/*                             class fits_in                                  */
/*****************************************************************************/

class fits_in : public ska_source_file_base {
  public: // Member functions
    ~fits_in();
    void read_header(jp2_family_tgt &tgt, kdu_args &args,
        ska_source_file* const source_file);
    void read_stripe(int height, float *buf,
        ska_source_file* const source_file, int component);
  private: // Members describing the organization of the FITS data
    void determine_min_and_max(ska_source_file* const source_file, int component);
    
    fitsfile *in;     //pointer to open FITS image
    int status;    // returned status of FITS functions
    fits_param fits;  // specific FITS parameters
    LONGLONG *fpixel; // array used by CFITSIO to specify starting pixel to read from.
    LONGLONG current_frame; // holds the currently read fream
    int nkeys;  //fits keywords
    char keyname[FLEN_CARD];  //fits
    char keyvalue[FLEN_CARD];  //fits
    char keycomment[FLEN_CARD];  //fits
    char record[FLEN_CARD];
    bool parse_fits_parameters(kdu_args &args);
    int scale;
    int bitpix;
    int type;
    int naxis;
    long* frame_fheight;
    int num_unread_rows;
};

/*****************************************************************************/
/*                             class fits_out                                */
/*****************************************************************************/

class fits_out : public ska_dest_file_base {
  public: // Public functions
    ~fits_out();
    void write_header(jp2_family_src &src, kdu_args &args,
        ska_dest_file* const dest_file);
    void write_stripe(int height, float *buf,
        ska_dest_file* const dest_file, int component);
  private: // Private functions
    bool parse_fits_parameters(kdu_args &args);
  private: // FITS file descriptions 
    fitsfile *out;     //pointer to open FITS image
    int status;    // returned status of FITS functions
    LONGLONG *fpixel; // array used by CFITSIO to specify starting pixel to read from.
    int nkeys;  //fits keywords
    int scale;
    int bitpix;
    int type;
    int naxis;
    long* frame_fheight;
    char keyname[FLEN_CARD];  //fits
    char keyvalue[FLEN_CARD];  //fits
    char keycomment[FLEN_CARD];  //fits

  private: // Writing descriptions
    int num_unwritten_rows;
};
