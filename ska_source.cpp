//SKA includes
#include "ska_local.h"
#include "hdf5_local.h"
#include "fits_local.h"
//testing includes
#include <iostream>

/*****************************************************************************/
/*                      ska_source_file::read_header                         */
/*****************************************************************************/

void
  ska_source_file::read_header(jp2_family_tgt &tgt, kdu_args &args) 
{
  std::cout << "parsing SKA parameters" << std::endl;
  parse_ska_args(tgt, args);
  std::cout << "SKA parameters parsed" << std::endl;
  const char *suffix;
  std::cout << "crop.height " << crop.height << std::endl;
  in = NULL;
  if ((suffix = strchr(fname, '.')) != NULL) {
    std::cout << suffix << std::endl;
    if ((strcmp(suffix+1,"h5")==0) || (strcmp(suffix+1,"H5")==0)) {
      in = new hdf5_in();
      in->read_header(tgt, args, this);
    }
    if ((strcmp(suffix+1,"fits")==0 || (strcmp(suffix+1,"FITS")==0)) ||
        (strcmp(suffix+1,"imfits")==0 || (strcmp(suffix+1,"IMFITS")==0))) {
      in = new fits_in();
      in->read_header(tgt, args, this);
    }
  }
  if (in == NULL)
  { kdu_error e; e << "Image file, \"" << fname << ", does not have a "
      "recognized suffix.  Valid suffices are currently: h5 and fits. "
      "Upper or lower case may be used, but must be used consistently."; }

  std::cout << "fname " << fname << std::endl;
  std::cout << "bytes_per_sample " << bytes_per_sample << std::endl;
  std::cout << "forced_prec " << forced_prec << std::endl;
  std::cout << "precision " << precision << std::endl;
  std::cout << "reversible " << reversible << std::endl;
  std::cout << "signed " << is_signed << std::endl;
  std::cout << "crop.height " << crop.height << std::endl;
  std::cout << "crop.width " << crop.width << std::endl;
}

/*****************************************************************************/
/*                        ska_source_file::read_stripe                       */
/*****************************************************************************/

void
  ska_source_file::read_stripe(int height, float *buf)
{
  // "this" is passed as a method of mimicing inheritance
  // the reason why have to do it this way is because "kdu_buffered_compress"
  // won't know which implementation to use for which file format. And we want
  // to minimize any changes we make to the app, as new versions of Kakadu
  // may be released.
  in->read_stripe(height, buf, this);
}

/*****************************************************************************/
/*                      ska_source_file::parse_ska_args                      */
/*****************************************************************************/

void
  ska_source_file::parse_ska_args(jp2_family_tgt &tgt, kdu_args &args)
{
  if (args.get_first() != NULL) {

    if (args.find("-icrop") != NULL)
    {
      crop.specified = true;
      const char *field_sep, *string = args.advance();
      for (field_sep=NULL; string != NULL; string=field_sep)
      {
        if (field_sep != NULL)
        {
          if (*string != ',')
          { kdu_error e; e << "\"-icrop\" argument requires a comma-"
            "separated list of cropping specifications."; }
            string++; // Walk past the separator
        }
        if (*string == '\0')
          break;
        if (((field_sep = strchr(string,'}')) != NULL) &&
            (*(++field_sep) == '\0'))
          field_sep = NULL;
        if ((sscanf(string,"{%d,%d,%d,%d,%d}", &(crop.x), &(crop.y), &(crop.z),
                &(crop.width), &(crop.height)) != 5) ||
                crop.x < 0 || crop.y < 0 || crop.z < 0 ||
                crop.width <= 0 || crop.height <= 0)
        { kdu_error e; e << "\"-icrop\" argument contains malformed "
          "cropping specification.  Expected to find five comma-separated "
            "integers, enclosed by curly braces.  The first three (x, y and z "
            "offsets must be non-negative) and the last two (width and "
            "height) must be strictly positive."; }
      }
      args.advance();
    }

    if (args.find("-fprec") != NULL) {
      char *string = args.advance();
      if (string == NULL)
      { kdu_error e; e << "Malformed `-fprec' argument.  Expected a comma "
        "separated list of non-negative forced precision values, each of "
          "which may optionally be followed by at most an `M' suffix."; }    
        int val = 0;
        if ((sscanf(string,"%d",&val) != 1) || (val < 0))
        { kdu_error e; e << "Malformed `-fprec' argument.  Expected a "
          "non-negative integer value."; }    
          forced_prec = val;

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
        if (!succ || (i == 0 && (sscanf(string, "%f", &float_minvals) != 1)))
          succ = false;
        else if (!succ || (i == 1 && 
              (sscanf(string, "%f", &float_maxvals) != 1)))
          succ = false;

        if (!succ)
        { kdu_error e; e << "\"-minmax\" argument contains "
          "malformed specification. Expected to find two comma-"
            "separated float numbers, enclosed by curly braces. "
            "Example: -minmax {-1.0,1.0}"; }
      }
      args.advance();
    }
    else {
      float_minvals = H5_FLOAT_MIN;
      float_maxvals = H5_FLOAT_MAX;
    }

    std::cout << "constructing meta data box" << std::endl;
    /* Put import parameter details into JPX header as a reference */
    /* TODO: i just need to read the book on what the different box type
     * integers are: otherwise this should be easy enough to get to work in its
     * current state.
    kdu_byte h5_uuid[16] = {0x72,0xF7,0x1C,0x30,
                            0x70,0x09,0x11,0xE2,
                            0xBC,0xFD,0x08,0x00,
                            0x20,0x0C,0x9A,0x66};

    std::cout << "copying over min and max" << std::endl;
    kdu_byte* contents = new kdu_byte [BUFSIZ]; // Should be plenty
    int contents_idx = 0, len = 0;
    char* tmp = new char [128];
    len = sprintf(tmp, "minfloat:%f,", float_minvals);
    for (int i = 0; i < len; ++i, ++contents_idx)
      contents[contents_idx] = tmp[i];
    len = sprintf(tmp, "maxfloat:%f,", float_maxvals);
    for (int i = 0; i < len; ++i, ++contents_idx)
      contents[contents_idx] = tmp[i];
    len = --contents_idx;
    std::cout << "copying complete" << std::endl;

    jp2_output_box out = jp2_output_box();
    out.open(&tgt, , false, false); 
    std::cout << out.get_box_type() << std::endl;
    out.set_target_size(16 + len);
    out.write(h5_uuid, 16); // unique identifier
    out.write(contents, len);
    if (!out.close())
    { kdu_error e; e << "Could not flush box to header."; }
    delete[] contents;
    delete[] tmp;
    */
  }
}
