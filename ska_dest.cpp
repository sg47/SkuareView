#include "ska_local.h"
#include "hdf5_local.h"
#include "fits_local.h"

/*****************************************************************************/
/*                      ska_dest_file::write_header                         */
/*****************************************************************************/

void
ska_dest_file::write_header(jp2_family_src &src, kdu_args &args) 
{
  parse_ska_args(src, args);
  const char *suffix;
  out = NULL;
  if ((suffix = strchr(fname, '.')) != NULL) {
    if ((strcmp(suffix+1,"h5")==0) || (strcmp(suffix+1,"H5")==0)) {
      //out = new hdf5_out();
      //out->write_header(src, args, this);
      kdu_error e; 
      e << "hdf5 out not ported yet!";
    }
    else if ((strcmp(suffix+1,"fits")==0) || (strcmp(suffix+1,"FITS")==0)) {
      out = new fits_out();
      out->write_header(src, args, this);
    }
  }
  if (out == NULL)
  { kdu_error e; e << "Image file, \"" << fname << ", does not have a "
    "recognized suffix.  Valid suffices are currently: h5. Upper or lower "
      "case may be used, but must be used consistently."; }
}

/*****************************************************************************/
/*                        ska_dest_file::write_stripe                       */
/*****************************************************************************/

void
ska_dest_file::write_stripe(int height, float *buf)
{
  // "this" is passed as a method of mimicing inheritance
  // the reason why have to do it this way is because "kdu_buffered_compress"
  // won't know which implementation to use for which file format. And we want
  // to minimize any changes we make to the app, as new versions of Kakadu
  // may be released.
  out->write_stripe(height, buf, this);
}

/*****************************************************************************/
/*                      ska_dest_file::parse_ska_args                      */
/*****************************************************************************/

void
  ska_dest_file::parse_ska_args(jp2_family_src &src, kdu_args &args)
{
  //TODO: read JP2 header
}
