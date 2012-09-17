/*****************************************************************************/
//
//  @file: fits_image_local.h
//  Project: fits_to_j2k
//
//  @author Slava Kitaeff
//  @date 29/07/12.
//  @brief The The file contains the definitions of types and classes
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
// This code is based on original 
// File: image_local.h [scope = APPS/IMAGE-IO]
// Version: Kakadu, V7.0
// Author: David Taubman
// Last Revised: 19 Jan, 2012
/*****************************************************************************/
// Copyright 2001, David Taubman, The University of New South Wales (UNSW)
// The copyright owner is Unisearch Ltd, Australia (commercial arm of UNSW)
// Neither this copyright statement, nor the licensing details below
// may be removed from this file or dissociated from its contents.
/*****************************************************************************/
// Licensee: International Centre For Radio Astronomy Research, Uni of WA
// License number: 01265
// The licensee has been granted a UNIVERSITY LIBRARY license to the
// contents of this source file.  A brief summary of this license appears
// below.  This summary is not to be relied upon in preference to the full
// text of the license agreement, accepted at purchase of the license.
// 1. The License is for University libraries which already own a copy of
//    the book, "JPEG2000: Image compression fundamentals, standards and
//    practice," (Taubman and Marcellin) published by Kluwer Academic
//    Publishers.
// 2. The Licensee has the right to distribute copies of the Kakadu software
//    to currently enrolled students and employed staff members of the
//    University, subject to their agreement not to further distribute the
//    software or make it available to unlicensed parties.
// 3. Subject to Clause 2, the enrolled students and employed staff members
//    of the University have the right to install and use the Kakadu software
//    and to develop Applications for their own use, in their capacity as
//    students or staff members of the University.  This right continues
//    only for the duration of enrollment or employment of the students or
//    staff members, as appropriate.
// 4. The enrolled students and employed staff members of the University have the
//    right to Deploy Applications built using the Kakadu software, provided
//    that such Deployment does not result in any direct or indirect financial
//    return to the students and staff members, the Licensee or any other
//    Third Party which further supplies or otherwise uses such Applications.
// 5. The Licensee, its students and staff members have the right to distribute
//    Reusable Code (including source code and dynamically or statically linked
//    libraries) to a Third Party, provided the Third Party possesses a license
//    to use the Kakadu software, and provided such distribution does not
//    result in any direct or indirect financial return to the Licensee,
//    students or staff members.  This right continues only for the
//    duration of enrollment or employment of the students or staff members,
//    as appropriate.
/******************************************************************************
Description:
   Local class definitions used by the implementation in "image_in.cpp" and
"image_out.cpp".  These should not be included from any other scope.
******************************************************************************/

#ifndef IMAGE_LOCAL_H
#define IMAGE_LOCAL_H

#include <stdio.h> // C I/O functions can be quite a bit faster than C++ ones
#include "kdu_elementary.h"
#include "kdu_image.h"
#include "kdu_file_io.h"
#include "kdu_tiff.h" // Required only for writing TIFF files

#include "fitsio.h"


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
    NONE, // no scaling
	LOG, // Logarithmic scale
	NEGATIVE_LOG, // Inverse image from logarithmic scale
	LINEAR, // Linear scale
	NEGATIVE_LINEAR, // Inverse image from linear scale
	RAW, // Convert raw values to image intensities.  
    // Only defined for FITS files containing short/byte data.  
    // If raw data is signed, it will be shifted to be unsigned.
	NEGATIVE_RAW, // Inverse image from raw transform
	SQRT, // Square root scale
	NEGATIVE_SQRT, // Inverse image from square root scale
	SQUARED, // Squared scale
	NEGATIVE_SQUARED, // Inverse image from squared scale
	POWER, // Power scale
	NEGATIVE_POWER, // Inverse image from power scale
	DEFAULT // Default transform to use if no transform is explicitly specified.  
    // This will depend on the FITS data type.
} transform;


typedef struct {
    transform transform; // the transform to be performed on the raw data.  
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
} fits_param;


/**
 * Structure for defining essential properties of a FITS datacube.
 */
typedef struct {
    int type;
	long width; // Image width
	long height; // Image height
	LONGLONG depth; // Image depth.  Arbitrary for 2D images
	LONGLONG stokes; // Number of stokes in image.  Arbitrary for 2D or 3D images
	int naxis; // Number of dimensions of the data cube
	int bitpix; // Image data type.  Same as BITPIX in CFITSIO
} cube_info;

// Defined here.

struct image_line_buf;
class pgm_in;
class pgm_out;
class ppm_in;
class ppm_out;
class raw_in;
class raw_out;
struct bmp_header;
class bmp_in;
class bmp_out;
class tif_in;
class fits_in;


#define KDU_IPTC_TAG_MARKER ((kdu_byte) 0x1c)

/*****************************************************************************/
/*                            image_line_buf                                 */
/*****************************************************************************/

struct image_line_buf {
  public: // Member functions
    image_line_buf(int width, int sample_bytes, int min_bytes=0)
      {
        this->width = width;
        this->sample_bytes = sample_bytes;
        this->buf_bytes = width*sample_bytes;
        if ((min_bytes > 0) && (buf_bytes < min_bytes)) buf_bytes = min_bytes;
        this->buf = new kdu_byte[buf_bytes];
        next = NULL;
        accessed_samples = 0;
        next_x_tnum = 0;
      }
    ~image_line_buf()
      { delete[] buf; }
  public:  // Data
    kdu_byte *buf;
    int buf_bytes;
    int sample_bytes;
    int width;
    int accessed_samples;
    int next_x_tnum;
    image_line_buf *next;
  };
  /* Notes:
     This structure provides a convenient mechanism for buffering
     tile-component lines as they are generated or consumed.  The `width'
     field indicates the number of samples in the line, while `sample_bytes'
     indicates the number of bytes used to represent each sample (the most
     significant byte always comes first).  The `accessed_samples' field
     indicates the number of samples which have already been read from or
     written into the line.  The `next_x_tnum' field holds the value of the
     `x_tnum' argument which should be expected in the next `get' or `put'
     call relevant to this line.  The `next' field may be used to build a
     linked list of buffered image lines. */


/*****************************************************************************/
/*                             class pbm_in                                  */
/*****************************************************************************/

class pbm_in : public kdu_image_in_base {
  public: // Member functions
    pbm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           kdu_rgb8_palette *palette, kdu_long skip_bytes);
    ~pbm_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int comp_idx;
    int rows, cols;
    image_line_buf *incomplete_lines; // Points to first incomplete line.
    image_line_buf *free_lines; // List of line buffers not currently in use.
    int num_unread_rows;
    FILE *in;
    bool forced_align_lsbs; // Relevant only if `forced_prec' is non-zero
    int forced_prec; // Non-zero if the precision has been forced
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class pgm_in                                  */
/*****************************************************************************/

class pgm_in : public kdu_image_in_base {
  public: // Member functions
    pgm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           kdu_long skip_bytes);
    ~pgm_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int comp_idx;
    int rows, cols;
    image_line_buf *incomplete_lines; // Points to first incomplete line.
    image_line_buf *free_lines; // List of line buffers not currently in use.
    int num_unread_rows;
    int forced_prec; // Non-zero if the precision has been forced
    bool forced_align_lsbs; // Irrelevant unless `forced_prec' is non-zero
    FILE *in;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class pgm_out                                 */
/*****************************************************************************/

class pgm_out : public kdu_image_out_base {
  public: // Member functions
    pgm_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx);
    ~pgm_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int comp_idx;
    int rows, cols;
    int precision;
    int orig_precision; // Same as above unless `precision' is being forced
    bool forced_align_lsbs; // Irrelevant if `precision' == `orig_precision'
    bool orig_signed; // True if original representation was signed
    image_line_buf *incomplete_lines; // Points to first incomplete line.
    image_line_buf *free_lines; // List of line buffers not currently in use.
    int num_unwritten_rows;
    FILE *out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class ppm_in                                  */
/*****************************************************************************/

class ppm_in : public kdu_image_in_base {
  public: // Member functions
    ppm_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           kdu_long skip_bytes);
    ~ppm_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int first_comp_idx;
    int rows, cols;
    image_line_buf *incomplete_lines; // Each "sample" consists of 3 bytes.
    image_line_buf *free_lines;
    int num_unread_rows;
    FILE *in;
    int forced_prec[3]; // Non-zero if the precision has been forced
    bool forced_align_lsbs[3]; // Irrelevant unless `forced_prec' val non-zero
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class ppm_out                                 */
/*****************************************************************************/

class ppm_out : public kdu_image_out_base {
  public: // Member functions
    ppm_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx);
    ~ppm_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int first_comp_idx;
    int rows, cols;
    int precision[3];
    int orig_precision[3]; // Same as above unless `precision' is being forced
    bool forced_align_lsbs[3]; // Irrelevant if `precision'=`orig_precision'
    bool orig_signed; // True if original representation was signed
    image_line_buf *incomplete_lines; // Each "sample" consists of 3 bytes.
    image_line_buf *free_lines;
    int num_unwritten_rows;
    FILE *out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class raw_in                                  */
/*****************************************************************************/

class raw_in : public kdu_image_in_base {
  public: // Member functions
    raw_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           kdu_long skip_bytes, bool littlendian);
    ~raw_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int comp_idx;
    int rows, cols;
    int precision;
    int sample_bytes;
    bool is_signed;
    image_line_buf *incomplete_lines;
    image_line_buf *free_lines;
    int num_unread_rows;
    FILE *in;
    int forced_prec; // Non-zero if precision is being forced
    bool forced_align_lsbs; // Irrelevant unless `forced_prec' is non-zero
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
    bool littlendian;
  };

/*****************************************************************************/
/*                             class raw_out                                 */
/*****************************************************************************/

class raw_out : public kdu_image_out_base {
  public: // Member functions
    raw_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
            bool littlendian);
    ~raw_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int comp_idx;
    int rows, cols;
    int precision;
    int orig_precision; // Equals above unless `precision' is being forced
    int sample_bytes;
    bool forced_align_lsbs; // Irrelevant if `precision' = `orig_precision'
    bool is_signed;
    image_line_buf *incomplete_lines;
    image_line_buf *free_lines;
    int num_unwritten_rows;
    FILE *out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
    bool littlendian;
  };

/*****************************************************************************/
/*                              bmp_header                                   */
/*****************************************************************************/

struct bmp_header {
    kdu_uint32 size; // Size of this structure: must be 40
    kdu_int32 width; // Image width
    kdu_int32 height; // Image height; -ve means top to bottom.
    kdu_uint32 planes_bits; // Planes in 16 LSB's (must be 1); bits in 16 MSB's
    kdu_uint32 compression; // Only accept 0 here (uncompressed RGB data)
    kdu_uint32 image_size; // Can be 0
    kdu_int32 xpels_per_metre; // We ignore these
    kdu_int32 ypels_per_metre; // We ignore these
    kdu_uint32 num_colours_used; // Entries in colour table (0 = use default)
    kdu_uint32 num_colours_important; // 0 means all colours are important.
  };
  /* Notes:
        This header structure must be preceded by a 14 byte field, whose
     first 2 bytes contain the string, "BM", and whose next 4 bytes contain
     the length of the entire file.  The next 4 bytes must be 0. The final
     4 bytes provides an offset from the start of the file to the first byte
     of image sample data.
        If the bit_count is 1, 4 or 8, the structure must be followed by
     a colour lookup table, with 4 bytes per entry, the first 3 of which
     identify the blue, green and red intensities, respectively. */

/*****************************************************************************/
/*                             class bmp_in                                  */
/*****************************************************************************/

class bmp_in : public kdu_image_in_base {
  public: // Member functions
    bmp_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           bool &vflip, kdu_rgb8_palette *palette, kdu_long skip_bytes);
    ~bmp_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private:
    void map_palette_index_bytes(kdu_byte *buf, bool absolute);
    void map_palette_index_nibbles(kdu_byte *buf, bool absolute);
    void map_palette_index_bits(kdu_byte *buf, bool absolute);
  private: // Data
    int first_comp_idx;
    int num_components;
    bool bytes, nibbles, bits; // Only when reading palette data
    bool expand_palette; // True if palette is to be applied while reading.
    kdu_byte map[1024];
    int precision; // Precision of samples after any palette mapping
    int rows, cols;
    int line_bytes; // Number of bytes in a single line of the BMP file.
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unread_rows;
    FILE *in;
    int forced_prec[4];
    bool forced_align_lsbs[4];
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };
  /* Notes:
        `num_components' is either 1, 3 or 4.  It holds the number of image
     components produced by the reader.  If  `bytes', `nibbles' or `bits'
     is true, the file contains palette indices, which must be unpacked
     (except when the indices are bytes) and mapped.
        If `expand_palette' is true, the palette indices must be mapped
     directly to RGB values and the `map' array holds interleaved palette
     information.  Each group of four bytes corresponds to a single palette
     index: the first byte is the blue colour value, the second is green, the
     third is red.  In this case, `num_components' may be 3 or 1.  If 1,
     the palette is monochrome and only one component will be expanded.
        If `expand_palette' is false, the number of components must be 1.  In
     this case, the palette indices themselves are to be compressed as a
     single image component; however, they must first be subjected to a
     permutation mapping, which rearranges the palette in a manner more
     amenable to compression.  In this case, the palette indices are applied
     directly to the `map' lookup table, which returns the mapped index in
     its low order 1, 4 or 8 bits, as appropriate. */

/*****************************************************************************/
/*                             class bmp_out                                 */
/*****************************************************************************/

class bmp_out : public kdu_image_out_base {
  public: // Member functions
    bmp_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx);
    ~bmp_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Data
    int first_comp_idx;
    int num_components;
    int rows, cols;
    int alignment_bytes; // Number of 0's at end of each line.
    int precision[3];
    int orig_precision[3]; // Same as above unless `precision' is being forced
    bool forced_align_lsbs[3]; // Ignored if `precision' == `orig_precision'
    bool orig_signed; // True if original representation was signed
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unwritten_rows;
    FILE *out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

/*****************************************************************************/
/*                             class tif_out                                 */
/*****************************************************************************/

class tif_out : public kdu_image_out_base {
  public: // Member functions
    tif_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
            bool quiet);
    ~tif_out();
    void put(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Helper functions
    void perform_buffer_pack(kdu_byte *buf);
  private: // Data
    int first_comp_idx;
    int num_components;
    int rows, cols;
    int precision;
    bool forced_align_lsbs;
    int *orig_precision; // All equal to above unless `precision' is forced.
    bool *is_signed; // One entry for each component
    int scanline_width, unpacked_samples;
    int sample_bytes, pixel_bytes, row_bytes;
    bool pre_pack_littlendian; // Scanline byte order prior to packing
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int num_unwritten_rows;
    kdu_simple_file_target out;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
  };

#ifdef KDU_INCLUDE_TIFF
#  include "tiffio.h"
#endif // KDU_INCLUDE_TIFF

/*****************************************************************************/
/*                             class tif_in                                  */
/*****************************************************************************/

class tif_in : public kdu_image_in_base {
  public: // Member functions
    tif_in(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
           kdu_rgb8_palette *palette, kdu_long skip_bytes, bool quiet);
    ~tif_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
  private: // Helper functions
    void perform_buffer_unpack(kdu_byte *src, kdu_byte *dst, int x_tile_idx);
      /* Works on a single chunk line.  `x_tile_idx' is the absolute index
         of the horizontal tile (starting from 0 for the first tile in the
         original image) from which the data was sourced.  This affects the
         number of samples and bytes which are processed. */
    void perform_sample_remap(kdu_byte *buf, int x_tile_idx);
      /* Works on a single chunk line, affecting only the first component.
         Again, `x_tile_idx' is the absolute index of the horizontal tile
         (starting from 0 for the first tile in the original image) from
         which the data was sourced. */
    void perform_palette_expand(kdu_byte *buf);
      /* Works on an entire scan-line */
  //--------------------------------------------------------------------------
  private: // Members describing the organization of the TIFF data
    bool *is_signed; // One entry for each image component
    double *float_minvals; // When TIFF file contains floating-point samples
    double *float_maxvals; // When TIFF file contains floating-point samples
    kdu_uint16 bitspersample;
    kdu_uint16 samplesperpixel;
    int chunkline_samples; // Number of unpacked samples in a row of a "chunk"
    int chunkline_bytes; // Number of raw bytes in a single row of a "chunk"
    int chunks_across; // Number of chunks across the original image width
    bool need_buffer_unpack; // True if `bitspersample' is not 8, 16 or 32.
    bool expand_palette; // True if palette is to be applied while reading.
    bool remap_samples; // True if palette is to be re-arranged.
    kdu_uint32 tile_width; // Image width if not tiled
    kdu_uint32 tile_height; // Strip height if not tiled
    int tiles_across; // 1 if not tiled
    int tiles_down; // Number of vertical strips if not tiled
    kdu_long *chunk_offsets; // NULL if `libtiff_in' used -- see below.
    int chunks_per_component; // Always equal to `tiles_across'*`tiles_down'
    kdu_simple_file_source src;
    int rows_at_a_time; // Number of rows to read at a time
    bool horizontally_tiled_tiff; // If horizontally tiled
#ifdef KDU_INCLUDE_TIFF
    TIFF *libtiff_in;
    kdu_byte *chunk_buf; // Used to buffer entire chunks, where necessary
#endif // KDU_INCLUDE_TIFF
  //--------------------------------------------------------------------------
  private: // Members which are affected by (or support) cropping
    int skip_rows; // Number of initial rows to skip from original image
    int skip_cols; // Number of initial cols to skip from original image
    int skip_tiles_across; // Same as `skip_cols'/`tile_width'
    int first_tile_skip_cols; // Columns to skip in the left-most tile
    int first_tile_width; // Number of columns used from left-most tile
    int last_tile_width; // Number of columns used from right-most tile
    int used_tiles_across; // Number of horizontal tiles we actually need
    int first_chunkline_skip_samples;
    int first_chunkline_samples; // For left-most tile
    int last_chunkline_samples;  // For right-most tile
    int first_chunkline_skip_bytes; // Bytes to skip over in left-most tile
    int first_chunkline_bytes; // For left-most tile
    int last_chunkline_bytes;  // For right-most tile
    int total_chunkline_bytes; // Total chunkline bytes in `used_tiles_across'
    kdu_long last_seek_addr; // So we can easily seek over unneeded data
  //--------------------------------------------------------------------------
  private: // Members which describe the image samples after they have been
           // extracted from the TIFF file and transformed, as required.
    int first_comp_idx;
    int num_components; // May be > `samplesperpixel' if there is a palette
    int precision;
    int *forced_prec; // One entry per component; non-zero if precision forced
    bool *forced_align_lsbs; // Ignored unless `forced_prec' val is non-zero
    int sample_bytes; // After expanding any palette
    int pixel_gap; // Gap between pixels (in bytes) after expanding any palette
    bool planar_organization; // See below
    bool invert_first_component; // If min value corresponds to white samples.
    kdu_byte map[1024]; // Used when expanding a palette
    int rows, cols;  // Dimensions after applying any cropping
    int num_unread_rows; // Always starts at `rows', even with cropping
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
    bool post_unpack_littlendian; // Byte order of data in `image_line_buf's
  };
  /* Notes:
        For TIFF files which use a contiguous (interleaved) organization, the
     meaning of a "chunk" is a single tile (or vertical strip, for non-tiled
     files).  For TIFF files which use a planar organization, the meaning of a
     "chunk" is a single image component (plane) of a single tile (or strip).
     The `chunk_offsets' array contains the locations in the TIFF source file
     at which each chunk is located.  Where a planar organization is used,
     the locations of all tiles (or strips) for component 0 appear first,
     followed by the locations of all chunks associated with component 1
     and so forth.
        The `planar_organization' field identifies the organization of each
     `image_line_buf' entry in the list headed by `incomplete_lines'.  If
     `planar_organization' is false, each `image_line_buf' line contains
     interleaved samples from all the components.  Otherwise, each
     `image_line_buf' line contains all samples from component 0,
     followed by all samples from component 1, and so forth. */

/*****************************************************************************/
/*                             class fits_in                                  */
/*****************************************************************************/

class fits_in : public kdu_image_in_base {
public: // Member functions
    fits_in(const char *fname, kdu_args &args, kdu_image_dims &dims,  
            int &next_comp_idx, bool &vflip, kdu_rgb8_palette *palette);
    ~fits_in();
    bool get(int comp_idx, kdu_line_buf &line, int x_tnum);
private: // Members describing the organization of the FITS data
    double *float_minvals; // When FITS file contains floating-point samples
    double *float_maxvals; // When FITS file contains floating-point samples
    kdu_uint16 bitspersample;
    kdu_simple_file_source src;
    fitsfile *in;     //pointer to open FITS image
    int status;    // returned status of FITS functions
	cube_info cinfo;  // FITS specific info
    fits_param fits;  // specific FITS parameters
	LONGLONG *fpixel; // array used by CFITSIO to specify starting pixel to read from.
    LONGLONG current_frame; // holds the currently read fream
    int current_stoke; // holds the currently read stoke
    int nkeys;  //fits keywords
    char keyname[FLEN_CARD];  //fits
    char keyvalue[FLEN_CARD];  //fits
    char keycomment[FLEN_CARD];  //fits
    bool parse_fits_parameters(kdu_args &args);
    int first_comp_idx;
    LONGLONG num_bytes;  // Number of bytes to read from FITS to the buffer
    int num_components; // May be > `samplesperpixel' if there is a palette
    int precision;
    int bytesample;
    int sample_bytes; // After expanding any palette
//    kdu_byte map[1024]; // Used when expanding a palette
    int rows, cols;  // Dimensions after applying any cropping
    int num_unread_rows; // Always starts at `rows', even with cropping
    int tilesout;
    LONGLONG planesout;
    image_line_buf *incomplete_lines; // Each "sample" represents a full pixel
    image_line_buf *free_lines;
    int scale;

//    int initial_non_empty_tiles; // tnum >= this implies empty; 0 until we know
    bool littlendian; // Byte order of data in `image_line_buf's
    //--------------------------------------------------------------------------
private: // Members which are affected by (or support) cropping
    int skip_rows; // Number of initial rows to skip from original image
    int skip_cols; // Number of initial cols to skip from original image
};

#endif // IMAGE_LOCAL_H
