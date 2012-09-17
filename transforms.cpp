/**
 * @file openjpeg.c
 * @author Andrew Cannon
 * @author Slava Kitaeff
 * @date December 2011
 *
 * @brief Command line parser
 *
 * Copyright 2012. International Centre for Radio Astronomy. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS `AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* ------------------------------------------------------------------------------------ */

#include <ctype.h>
#include <string.h>
#include <stdio.h> // so we can use `sscanf' for arg parsing.
#include <errno.h>

// Kakadu core includes
#include "kdu_arch.h"
#include "kdu_elementary.h"
#include "kdu_messaging.h"
#include "kdu_params.h"
#include "kdu_compressed.h"
#include "kdu_roi_processing.h"
#include "kdu_sample_processing.h"

#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#include "fc2jk.h"

/**
 * Percentage standard deviation of Gaussian noise to be generated in image.  Will be
 * 0.0 unless otheriwse specified by the user on the command line.  The noise defined by
 * this parameter will be added to the raw FITS values before they are transformed into
 * JPEG 2000 pixel intensities.  This is a percentage of the difference between the greatest
 * and least values in the raw FITS data.
 */
double gaussianNoisePctStdDeviation = 0.0;

/**
 * Macro to add Gaussian noise to raw floating point data and ensure that it still
 * remains within its known minimum and maximum values.
 */
#define ADD_GAUSSIAN_NOISE_TO_RAW_VALUES() {\
if (gaussianNoisePctStdDeviation >= 0.0000001 || gaussianNoisePctStdDeviation <= -0.0000001) {\
rawData[index] += (datamax-datamin) * getPctGaussianNoise();\
\
if (rawData[index] > datamax) {\
rawData[index] = datamax;\
}\
\
if (rawData[index] < datamin) {\
rawData[index] = datamin;\
}\
}\
}

/**
 * Macro to add Gaussian noise to integer pixel intensities, calculate the noise added and
 * add the square of this value to a cumulative total.
 *
 * @param max Maximum pixel intensity in the image.
 * @param noise_min Minimum noise value.  Usually (max+1)/2-1;
 * @param noise_max Maximum noise value.  Usually -(max+1)/2;
 */
#define ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(max,noise_min,noise_max) {\
int oldValue = imageData[ii];\
imageData[ii] += getIntegerGaussianNoise(NULL,NULL,NULL);\
FIT_TO_RANGE(0,max,imageData[ii]);\
int dif = imageData[ii]-oldValue;\
unsigned long long int absDif = (unsigned long long int) abs(dif);\
squareNoiseSum += absDif*absDif;\
if (writeNoiseField) {\
noiseData[ii] = dif;\
FIT_TO_RANGE(noise_min,noise_max,noiseData[ii]);\
}\
}

/**
 * Function to print out Gaussian noise benchmark, showing the actual PSNR in image after
 * noise has been added and the raw integer data used to calculate that value.
 *
 * @param max Maximum pixel intensity in the image.  Should be an integer.
 */
Print_Noise_Benchmark(unsigned long long int squareNoiseSum, 
                      size_t len, 
                      int max) {

fprintf(stdout,"[Squared Noise Sum] [Pixels] [Maximum Intensity] [PSNR with noise (dB)]\n");
fprintf(stdout,"%llu %zu %d ",squareNoiseSum,len,max);
if (squareNoiseSum > 0) {
    fprintf(stdout,"%f\n",10.0*log10(((double)len)*((double)max*max)/((double)squareNoiseSum)));
    }else{
        fprintf(stdout,"NO-PSNR\n");
        }
}


/**
 * Macro to truncate data values so that they lie inside a particular range.
 *
 * @param min Smallest permissible value.
 * @param max Largest permissible value.
 * @param var Variable to truncate.
 */
#define FIT_TO_RANGE(min,max,var) {\
if (var < min) {\
var = min;\
}\
else if (var > max) {\
var = max;\
}\
}

/**
 * Macro to update index enable vertical flipping of a FITS file after
 * it has been read.
 */
#define UPDATE_FLIPPING_INDEX() {\
index++;\
dif++;\
\
if (dif >= width) {\
dif = 0;\
index -= 2*width;\
}\
}

#ifdef noise
#define TRANSFORM_END ,writeNoiseField ? noiseField->comps[0].data : NULL,writeNoiseField,printNoiseBenchmark
#else
#define TRANSFORM_END
#endif


/**
 * Macro to encode an image losslessly.  Requires an integer, 'result' to be defined in the
 * same scope.  By reading this integer after this macro is run, it may be checked whether
 * compression was successful.
 *
 * @param image opj_image_t image structure that will be written to a
 * lossless JPEG 2000 file.
 * @param name String to append to output file name.
 * @param nameLength Length of the string name.
 * @param outFileStub Start of output file name.
 */
#define ENCODE_LOSSLESSLY(image,name,nameLength,outFileStub) {\
OPJ_CODEC_FORMAT losslessCodec = CODEC_JP2;\
opj_cparameters_t lossless;\
opj_set_default_encoder_parameters(&lossless);\
lossless.tcp_mct = 0;\
if (lossless.tcp_numlayers == 0) {\
lossless.tcp_rates[0] = 0;\
lossless.tcp_numlayers++;\
lossless.cp_disto_alloc = 1;\
}\
char losslessFile[stublen + 6 + nameLength];\
\
sprintf(losslessFile,"%s_" name ".jp2",outFileStub);\
\
result = createJPEG2000Image(losslessFile,losslessCodec,&lossless,&image);\
}


/**
 * Function that returns a noise value that may be added to a pixel intensity.  These values
 * are normally (Gaussian) distributed with a mean of 0 and standard deviation specified by
 * the user at the command line.  If no noise value is specified at the command line, this
 * function will always return 0.
 *
 * To generate (non-zero) noise, this function must be initialised with the maximum pixel
 * intensity that will appear in the image (255/65535 respectively for images scaled to occupy
 * the full intensity range of 8/16 bit images respectively) and the PSNR (in DB) of the image
 * after noise has been added.  An optional seed for the random number generator may also be
 * specified.  Until noiseDB and maxIntensity have been specified, the function will only return
 * 0.  Until both these values are set, any specified values of noiseDB, maxIntensity or seed will
 * be stored (overwriting any previous stored values) with each function call.  Once these two
 * values are specified, the parameters to the function are ignored and random variates will be
 * return when it is called.
 *
 * If no seed is specified, the random number generator is seeded with the system clock.
 *
 * @param noiseDB PSNR (in DB) of image after noise has been added (used to initialise function).
 * @param maxIntensity Maximum pixel intensity in the image (used to initialise function).
 * @param seed Need for random number generator (used to initialise function).
 *
 * @return Gaussian random variate with mean 0 and standard deviation gaussianNoiseStdDeviation
 */
int getIntegerGaussianNoise(double *noiseDB, int *maxIntensity, unsigned long int *seed) {
	// Have the static variables been properly setup to return Gaussian noise?
	static bool initialised = false;
    
	// PSNR (in dB) of image after noise has been added.
	static double db = 0.0;
	static bool noiseSet = false;
    
	// Standard deviation of noise to be generated (as a pixel intensity).
	static double noiseDev = 0.0;
    
	// Maximum pixel intensity in image.
	static int maxPixelIntensity = 0;
	static bool maxIntensitySet = false;
    
	// Random number generator seed.
	static unsigned long generatorSeed = 0;
	static bool seedSet = false;
    
	// Random number generator.
	static gsl_rng *r = NULL;
    
	if (initialised) {
		return gsl_ran_gaussian_ziggurat(r,noiseDev);
	}
	else {
		// Set random number generator seed.
		if (seed != NULL) {
			generatorSeed = *seed;
			seedSet = true;
		}
        
		// Set noise (in DB) to be added to image.
		if (noiseDB != NULL) {
			db = *noiseDB;
			noiseSet = true;
		}
        
		// Set maximum intensity of pixels in image.
		if (maxIntensity != NULL) {
			maxPixelIntensity = *maxIntensity;
			maxIntensitySet = true;
		}
        
		// Function becomes initialised to start producing random variates at
		// the point when the amount of noise (in DB) to be added and the maximum
		// image pixel intensity are set.
		if (noiseSet && maxIntensitySet) {
			initialised = true;
            
			// Create random number generator.
			// Allocate/initialise random number generator.
			// Using the Mersenne Twister - this could be changed if necessary.
			r = gsl_rng_alloc(gsl_rng_mt19937);
            
			// Check allocation was successful.
			if (r == NULL) {
				fprintf(stderr,"Unable to allocate memory for random number generator.\n");
				exit(EXIT_FAILURE);
			}
            
			// See if a particular seed was specified, otherwise seed with system time.
			if (seedSet == true) {
				gsl_rng_set(r,generatorSeed);
			}
			else {
				gsl_rng_set(r,time(NULL));
			}
            
			// Calculate standard deviation for Gaussian noise distribution.
			noiseDev = ((double) maxPixelIntensity) * pow(10.0,-0.05 * db);
		}
        
		// Return 0 in the case
		return 0;
	}
}

/**
 * Function that returns a floating point value with mean 0.  These values are normally
 * (Gaussian) distributed with a standard deviation specified by a user command line
 * parameter indicating a percentage (of the difference between the minimum and maximum
 * raw FITS values).  If no such value is specified at the command line, this function
 * will always return 0.0.
 *
 * @return Gaussian random variate with mean 0 and standard deviation gaussianNoisePctStdDeviation
 */
double getPctGaussianNoise() {
	// Always return 0.0 if the specified Gaussian noise distribution is close to 0.
	if (gaussianNoisePctStdDeviation < 0.0000001 && gaussianNoisePctStdDeviation > -0.0000001) {
		return 0.0;
	}
	else {
		// Random number generator.
		static gsl_rng *r = NULL;
        
		if (r == NULL) {
			// Allocate/initialise random number generator.
			// Using the Mersenne Twister - this could be changed if necessary.
			r = gsl_rng_alloc(gsl_rng_mt19937);
            
			// Check allocation was successful.
			if (r == NULL) {
				fprintf(stderr,"Unable to allocate memory for floating point random number generator.\n");
				exit(EXIT_FAILURE);
			}
            
			// Seed random number generator with system time.
			// Maybe the seed should be fixed in the name of getting reproducible results?
			// Offset time by 100 to ensure this does not match the random number generator
			// in getIntegerGaussianNoise().
			gsl_rng_set(r,time(NULL)+100);
		}
        
		// Return noise value.
		return gsl_ran_gaussian_ziggurat(r,gaussianNoisePctStdDeviation/100.0);
	}
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * a long long int array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * @param rawData long long int array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len - length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int longLongImgTransform(long long int *rawData, int *imageData, transform transform, size_t len, size_t width
                         , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	fprintf(stderr,"This data type is not currently supported.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * an int array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * @param rawData int array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int intImgTransform(int *rawData, int *imageData, transform transform, size_t len, size_t width
                    , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	fprintf(stderr,"This data type is not currently supported.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * an unsigned int array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * @param rawData unsigned int array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int uIntImgTransform(unsigned int *rawData, int *imageData, transform transform, size_t len, size_t width
                     , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	fprintf(stderr,"This data type is not currently supported.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * a short array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * Very basic parameter checking is performed, but the responsibility for checking
 * parameters are valid and meaningful is largely left to the calling function.
 *
 * @param rawData short array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int shortImgTransform(short *rawData, int *imageData, transform transform, size_t len, size_t width
                      , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	if (rawData == NULL || imageData == NULL || len < 1) {
		fprintf(stderr,"Data arrays to shortImgTransform cannot be null or empty.\n");
		return 1;
	}
    
	// Loop variables
	size_t ii;
    
	// Sum of the squared error introduced to image.
	unsigned long long int squareNoiseSum = 0;
    
	if (transform == RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Shift scales (from signed to unsigned) then do a 1-1 mapping.
		for (ii=0; ii<len; ii++) {
			imageData[ii] = (int) rawData[index] + 32768;
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == NEGATIVE_RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// As for linear, but subtract from 65535
		for (ii=0; ii<len; ii++) {
			imageData[ii] = 32767 - (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
    
	fprintf(stderr,"This scaling is not currently supported for this data type.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * an unsigned short array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * Very basic parameter checking is performed, but the responsibility for checking
 * parameters are valid and meaningful is largely left to the calling function.
 *
 * @param rawData unsigned short array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int uShortImgTransform(unsigned short *rawData, 
                       int *imageData, 
                       transform transform, 
                       size_t len, 
                       size_t width, 
                       int *noiseData, 
                       bool writeNoiseField, 
                       bool printNoiseBenchmark) {
	if (rawData == NULL || imageData == NULL || len < 1) {
		fprintf(stderr,"Data arrays to uShortImgTransform cannot be null or empty.\n");
		return 1;
	}
    
	// Loop variables
	size_t ii;
    
	// Sum of the squared error introduced to image.
	unsigned long long int squareNoiseSum = 0;
    
	if (transform == RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Simple raw copying.
		for (ii=0; ii<len; ii++) {
			imageData[ii] = (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == NEGATIVE_RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// As for linear, but subtract from 65535
		for (ii=0; ii<len; ii++) {
			imageData[ii] = 65535 - (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
    
	fprintf(stderr,"This transform is not currently supported for this data type.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * an unsigned char array) into grayscale image intensities (between 0 and 255 inclusive).
 *
 * Very basic parameter checking is performed, but the responsibility for checking
 * parameters are valid and meaningful is largely left to the calling function.
 *
 * @param rawData unsigned char array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int byteImgTransform(unsigned char *rawData, 
                     int *imageData, 
                     transform transform, 
                     size_t len, 
                     size_t width, 
                     int *noiseData, 
                     bool writeNoiseField, 
                     bool printNoiseBenchmark) {

	if (rawData == NULL || imageData == NULL || len < 1) {
		fprintf(stderr,"Data arrays to byteImgTransform cannot be null or empty.\n");
		return 1;
	}
    
	// Loop variables
	size_t ii;
    
	// Sum of the squared error introduced to image.
	unsigned long long int squareNoiseSum = 0;
    
	if (transform == RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Simple raw transform
		for (ii=0; ii<len; ii++) {
			imageData[ii] = (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(255,-128,127);
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 255);
		return 0;
	}
	else if (transform == NEGATIVE_RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Invert raw transform
		for (ii=0; ii<len; ii++) {
			imageData[ii] = 255 - (int) rawData[index];
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(255,-128,127);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 255);
		return 0;
	}
    
	fprintf(stderr,"This transform is not currently supported for this data type.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * a char array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * Very basic parameter checking is performed, but the responsibility for checking
 * parameters are valid and meaningful is largely left to the calling function.
 *
 * @param rawData signed char array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int sByteImgTransform(signed char *rawData, int *imageData, transform transform, size_t len, size_t width
                      , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	if (rawData == NULL || imageData == NULL || len < 1) {
		fprintf(stderr,"Data arrays to sByteImgTransform cannot be null or empty.\n");
		return 1;
	}
    
	// Loop variables
	size_t ii;
    
	// Sum of the squared error introduced to image.
	unsigned long long int squareNoiseSum = 0;
    
	if (transform == RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Take raw data, shift it to be unsigned.
		for (ii=0; ii<len; ii++) {
			imageData[ii] = 128 + (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(255,-128,127);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 255);
		return 0;
	}
	else if (transform == NEGATIVE_RAW) {
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		// Invert raw transform.
		for (ii=0; ii<len; ii++) {
			imageData[ii] = 127 + (int) rawData[index];
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(255,-128,127);
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 255);
		return 0;
	}
    
	fprintf(stderr,"This transform is not currently supported for this data type.\n");
	return 1;
}

/**
 * Function for transforming a raw array of data from a FITS file (in the form of
 * a double array) into grayscale image intensities (between 0 and 2^16-1 inclusive).
 *
 * Very basic parameter checking is performed, but the responsibility for checking that
 * parameters are valid and meaningful is largely left to the calling function.
 *
 * @param rawData double array read from a FITS file using CFITSIO
 * @param imageData int array, assumed to be the same length as rawData, to be populated
 * with grayscale image intensities.
 * @param transform transform to perform on each datum of rawData to get imageData.
 * @param len length of rawData & imageData arrays.
 * @param datamin minimum value in rawData.
 * @param datamax maximum value in rawData.
 * @param width width of image.
 * @param noiseData int array, assumed to be the same length as rawData, to be populated
 * with grayscale noise value intensities.  Will only be accessed if writeNoiseField is
 * set to true.  If the definition of noise is removed from f2j.h, this parameter will
 * disappear.
 * @param writeNoiseField Should noise data be written?  If the definition of noise is removed
 * from f2j.h, this parameter will disappear.
 * @param printNoiseBenchmark Should information on the actual PSNR achieved by adding noise to the image be displayed
 * to the user?  This parameter will disappear if the definition of noise is removed from f2j.h.
 *
 * @return 0 if the transform could be performed successfully, 1 otherwise.
 */
int floatDoubleTransform(double *rawData, int *imageData, transform transform, size_t len, double datamin, double datamax, size_t width
                         , int *noiseData, bool writeNoiseField, bool printNoiseBenchmark) {
	if (rawData == NULL || imageData == NULL || len < 1) {
		fprintf(stderr,"Data arrays in floatDoubleTransform cannot be null or empty.\n");
		return 1;
	}
    
	// Loop variables
	size_t ii;
    
	// Sum of the squared error introduced to image.
	unsigned long long int squareNoiseSum = 0;
    
	if (transform == LOG || transform == NEGATIVE_LOG) {
		double absMin = datamin;
		double zero = 0.0;
        
		if (datamin < 0.0) {
			absMin = -absMin;
			zero = 2*absMin;
		}
		else if (datamin <= 0.0) {
			absMin = 0.000001;
			zero = absMin;
		}
        
		double scale = 65535.0/log((datamax+zero)/absMin);
        
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		for (ii=0; ii<len; ii++) {
			ADD_GAUSSIAN_NOISE_TO_RAW_VALUES();
			// Read the flipped image pixel.
			imageData[ii] = (int) (scale * log( (rawData[index] + zero) / absMin) );
            
			// Shouldn't get values outside this range, but just in case.
			FIT_TO_RANGE(0,65535,imageData[ii]);
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			if (transform == NEGATIVE_LOG) {
				imageData[ii] = 65535 - imageData[ii];
			}
            
			UPDATE_FLIPPING_INDEX();
		}
        
		// Print (or don't print) noise simulation benchmarks.
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == LINEAR || transform == NEGATIVE_LINEAR) {
		double absMin = datamin;
		double zero = 0.0;
        
		if (datamin < 0.0) {
			absMin = -absMin;
			zero = absMin;
		}
        
		double scale = 65535.0/(datamax+zero);
        
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		for (ii=0; ii<len; ii++) {
			ADD_GAUSSIAN_NOISE_TO_RAW_VALUES();
			imageData[ii] = (int) (rawData[index] * scale);
			FIT_TO_RANGE(0,65535,imageData[ii]);
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			if (transform == NEGATIVE_LINEAR) {
				imageData[ii] = 65535 - imageData[ii];
			}
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == SQRT || transform == NEGATIVE_SQRT) {
		// Scale factor.
		double scale = 0.0;
        
		if (datamin != datamax) {
			scale = 65535.0/sqrt(datamax-datamin);
		}
        
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		for (ii=0; ii<len; ii++) {
			ADD_GAUSSIAN_NOISE_TO_RAW_VALUES();
			imageData[ii] = (int) (scale * sqrt(rawData[index]-datamin));
			FIT_TO_RANGE(0,65535,imageData[ii]);
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			if (transform == NEGATIVE_SQRT) {
				imageData[ii] = 65535 - imageData[ii];
			}
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == SQUARED || transform == NEGATIVE_SQUARED) {
		// Scale factor.
		double scale = 0.0;
        
		if (datamin != datamax) {
			scale = 65535.0/( (datamax-datamin)*(datamax-datamin) );
		}
        
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		for (ii=0; ii<len; ii++) {
			ADD_GAUSSIAN_NOISE_TO_RAW_VALUES();
			imageData[ii] = (int) (scale * (rawData[index]-datamin) * (rawData[index]-datamin));
			FIT_TO_RANGE(0,65535,imageData[ii]);
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			if (transform == NEGATIVE_SQUARED) {
				imageData[ii] = 65535 - imageData[ii];
			}
            
			UPDATE_FLIPPING_INDEX();
		}
        
        if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
	else if (transform == POWER || transform == NEGATIVE_POWER) {
		// Scale factor.
		double scale = 0.0;
        
		if (datamin != datamax) {
			scale = 65535.0/( exp(datamax) - exp(datamin) );
		}
        
		// Offset.
		double offset = 0.0;
        
		if (datamin != datamax) {
			offset = 65535.0 * exp(datamin) / ( exp(datamin) - exp(datamax) );
		}
        
		// Variables that enable us to flip the image vertically as we read it in.
		size_t index = len-width;
		size_t dif = 0;
        
		for (ii=0; ii<len; ii++) {
			ADD_GAUSSIAN_NOISE_TO_RAW_VALUES();
			imageData[ii] = (int) (scale * exp(rawData[index]) + offset);
			FIT_TO_RANGE(0,65535,imageData[ii]);
            
			ADD_GAUSSIAN_NOISE_TO_INTEGER_VALUES(65535,-32768,32767);
            
			if (transform == NEGATIVE_POWER) {
				imageData[ii] = 65535 - imageData[ii];
			}
            
			UPDATE_FLIPPING_INDEX();
		}
        
		if (printNoiseBenchmark) Print_Noise_Benchmark(squareNoiseSum, len, 65535);
		return 0;
	}
    
	fprintf(stderr,"This transform is not currently supported for this data type.\n");
	return 1;
}



