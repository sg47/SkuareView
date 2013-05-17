#ifndef SKA_SOURCE_H
#define SKA_SOURCE_H

#include <fstream>
#include "kdu_args.h"
#include "jp2.h"

/* Specifies the cropping of the input hdf5 file. Currently only 1 plane
 * is specified. Previously we had handled the 3rd dimension with components
 * however this approach is not feasible with the stripe compressor. The reason
 * being that the stripe compressor requires all components in a stripe. As SKA
 * cubes are anticipated to have 10 000s of planes this approach is not 
 * feasible. */
struct cropping {
  bool specified; // if false, use full extent of image
  int x;
  int y;
  int z; // Selection plane of the hdf5 cube
  int height;
  int width;
};

class ska_source_file;
class ska_source_file_base;
class ska_dest_file;
class ska_dest_file_base;

/*****************************************************************************/
/*                         class ska_source_file                             */
/*****************************************************************************/

class ska_source_file_base {
  /* Pure virtual base class. Provides an interface to derived classes which 
   * support reading of a specific file type. */
  public: // Single interface function.
    virtual ~ska_source_file_base() {}
    /* jp2_family_tgt is required to write metadata from file formats used 
     * by the SKA to the destination jpeg2000 file. We also require additional
     * arguments not offered by the Kakadu library so that we can richly encode
     * our images.  */
    virtual void read_header(jp2_family_tgt &tgt, kdu_args &args,
        ska_source_file* const source_file) = 0;
    virtual void read_stripe(int height, kdu_byte *buf, 
        ska_source_file* const source_file) = 0;
};

class ska_source_file {
  /* Allows one to readily add file formats encoders for Kakadu's
   * "stripe_compressor" and "stripe_decompressor". The structure is very similar
   * to that of "kdu_image_in." */
  public: // Member functions
    ska_source_file() {
      fname=NULL;
      fp=NULL;
      bytes_per_sample=1;
      precision=8;
      is_signed=is_raw=swap_bytes=false;
      size=kdu_coords(0,0);
      next=NULL;
    }
    ~ska_source_file() {
      if (fname != NULL) delete[] fname;
      if (fp != NULL) fclose(fp);
    }
    void read_header(jp2_family_tgt &tgt, kdu_args &args);
    void read_stripe(int height, kdu_byte *buf);
  private: // Private functions
    /* Parses generic arguments used by the SKA encoder */
    void parse_ska_args(jp2_family_tgt &tgt, kdu_args &args);
  private: // Private data
    class ska_source_file_base *in;
  public: // Data
    char *fname;
    FILE *fp;
    int bytes_per_sample;
    int precision; // Num bits
    bool is_signed;
    bool is_raw;
    bool swap_bytes; // If raw file word order differs from machine word order
    bool reversible;
    kdu_coords size; // Width, and remaining rows

    int forced_prec;
    cropping crop;
    /* These file streams are used for testing with the parameter -rawtest,
     * raw_before writes to "encoder_before_rawtest" with csv of the floats of
     * the input files.
     * raw after writes to "encoder_after_rawtest" with csv of the floats of
     * the inputs files after they have been normalized for jpeg2000. */
    std::ofstream raw_before, raw_after;
    float float_minvals; // Minimum float in input file
    float float_maxvals;

    //TODO
    /* While multiple files can be accepted as arguments, currently the
     * implementation only processes the first. */
    ska_source_file *next;
};

/*****************************************************************************/
/*                          class ska_dest_file                              */
/*****************************************************************************/

class ska_dest_file_base {
  /* Pure virtual base class. Provides an interface to derived classes which 
   * support writing to a specific file type. */ 
  public: // Single interface function.
    virtual ~ska_dest_file_base() {}
    /* jp2_family_tgt is required to write metadata from file formats used 
     * by the SKA to the destination jpeg2000 file. We also require additional
     * arguments not offered by the Kakadu library so that we can richly encode
     * our images.  */
    virtual void write_header(jp2_family_tgt &tgt, kdu_args &args,
        ska_dest_file* const dest_file) = 0;
    virtual void write_stripe(int height, kdu_byte *buf, 
        ska_dest_file* const dest_file) = 0;
};

class ska_dest_file {
  /* Allows one to readily add file formats encoders for Kakadu's
   * "stripe_compressor" and "stripe_decompressor". The structure is very
   * similar to that of "kdu_image_in." */
  public: // Member functions
    ska_dest_file() {
      fname=NULL;
      fp=NULL;
      bytes_per_sample=1;
      precision=8;
      is_signed=false;
      next=NULL; 
    }
    ~ska_dest_file() {
      if (fname != NULL) delete[] fname;
      if (fp != NULL) fclose(fp);
    }
    void write_header(jp2_family_tgt &tgt, kdu_args &args);
    void write_stripe(int height, kdu_byte *buf);
  private: // Private functions
    /* Parses generic arguments used by the SKA encoder */
    void parse_ska_args(jp2_family_tgt &tgt, kdu_args &args);
  private: // Private data
    class ska_dest_file_base *out;
  public: // Data
    char *fname;
    FILE *fp;
    int bytes_per_sample;
    int precision; // bit depth
    int forced_prec; // see -fprec
    bool little_endian;
    bool is_signed;

    int* dimensions; // JP2 image dimensions
    cropping crop; // cropping specified of the JP2 dimensions
    double samples_min, samples_max; // min/max values of all samples
    bool reversible; // reversible compression
    int num_unwritten_rows;
};

#endif
