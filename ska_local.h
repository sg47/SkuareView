#ifndef SKA_SOURCE_H
#define SKA_SOURCE_H

#include <fstream>
#include "kdu_args.h"
#include "jp2.h"
//testing includes
#include <iostream>

/* Specifies the cropping of the input hdf5 file. Currently only 1 plane
 * is specified. Previously we had handled the 3rd dimension with components
 * however this approach is not feasible with the stripe compressor. The reason
 * being that the stripe compressor requires all components in a stripe. As SKA
 * cubes are anticipated to have 10 000s of planes this approach is not 
 * feasible. */
struct cropping {
  int naxis;
  bool specified; // if false, use full extent of image
  int x;
  int y;
  int z; // Selection plane of the hdf5 cube
  int height;
  int width;
  int depth; // TODO: currently not being used
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
    virtual void read_stripe(int height, float *buf, 
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
      is_signed=false;
      reversible=false;
      float_minvals = -0.5;
      float_maxvals = 0.5;
    }
    ~ska_source_file() {
      if (fname != NULL) delete[] fname;
      if (fp != NULL) fclose(fp);
      std::cout << "did that go ok?" << std::endl;
    }
    void read_header(jp2_family_tgt &tgt, kdu_args &args);
    void read_stripe(int height, float *buf);
  private: // Private functions
    /* Parses generic arguments used by the SKA encoder */
    void parse_ska_args(jp2_family_tgt &tgt, kdu_args &args);
  private: // Private data
    class ska_source_file_base *in;
  public: // Data
    char *fname;
    FILE *fp;
    int bytes_per_sample;
    int forced_prec;
    int precision; // bit depth
    bool is_signed;
    bool reversible;

    int rank;
    int* extent;
    int* offset;
    cropping crop;
    double float_minvals, float_maxvals;
    int num_unread_rows;
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
    virtual void write_header(jp2_family_src &src, kdu_args &args,
        ska_dest_file* const dest_file) = 0;
    virtual void write_stripe(int height, float *buf, 
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
      reversible=false;
    }
    ~ska_dest_file() {
      if (fname != NULL) delete[] fname;
      if (fp != NULL) fclose(fp);
    }
    void write_header(jp2_family_src &src, kdu_args &args);
    void write_stripe(int height, float *buf);
  private: // Private functions
    /* Parses generic arguments used by the SKA encoder */
    void parse_ska_args(jp2_family_src &src, kdu_args &args);
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

    //TODO
    /* While multiple files can be accepted as arguments, currently the
     * implementation only processes the first. */
    ska_dest_file *next;
};

#endif
