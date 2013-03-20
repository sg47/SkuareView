/* Specifies the cropping of the input hdf5 file. Currently only 2 dimensions
 * are specified. Previously we had handled the 3rd dimension with components
 * however this approach is not feasible with the stripe compressor. The reason
 * being that the stripe compressor requires all components in a stripe. As SKA
 * cubes are anticipated to have 10 000s of planes this approach is not 
 * feasible. */
struct cropping {
  bool specified = false;
  int x;
  int y;
  int z; // Selection plane of the hdf5 cube
  int height;
  int width;
};

class ska_source_file;

/*****************************************************************************/
/*                         class ska_source_file                             */
/*****************************************************************************/

class ska_source_file {
  public: // Member functions
    ska_source_file() {
      fname=NULL;
      fp=NULL;
      bytes_per_samples=1;
      precision=8;
      is_signed=is_raw_bytes=false;
      size=kdu_coords(0,0);
      next=NULL;
    }
    ~ska_source_file() {
      if (fname != NULL) delete[] fname;
      if (fp != NULL) fclose(fp);
    }
    /* jp2_family_tgt is required to write metadata from file formats used 
     * by the SKA to the destination jpeg2000 file. We also require additional
     * arguments not offered by the Kakadu library so that we can richly encode
     * our images.  */
    void read_header(jp2_family_tgt &tgt, kdu_args &args);
    void read_stripe(int height, kdu_int16 *buf);
  private: 
    /* Parses generic arguments used by the SKA encoder */
    void parse_ska_args(kdu_args &args);
  public: // Data
    char *fname;
    FILE *fp;
    int bytes_per_sample;
    int precision; // Num bits
    bool is_signed;
    bool is_raw;
    bool swap_bytes; // If raw file word order differs from machine word order
    kdu_coords size; // Width, and remaining rows
    ska_source_file *next;

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
}
