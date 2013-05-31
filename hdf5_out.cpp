/*****************************************************************************/
//
//  @file: hdf5_out.cpp
//  Project: SkuareView-NGAS-plugin
//
//  @author Sean Peters
//  @date 18/02/2013
//  @brief Implements decoding from JPEG2000 to the HDF5 file format specified 
//         by ICRAR. Readily extendible to include further features.
// 
//  Copyright (c) 2012 University of Western Australia. All rights reserved.
//
// This code is based on original 
// File: image_in.cpp [scope = APPS/IMAGE-IO]
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
/******************************************************************************/

// System includes
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <string.h>
#include <math.h>
#include <assert.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
// Image includes
#include "kdu_image.h"
#include "image_local.h"
// HDF5 includes
#include "hdf5_local.h"

/*****************************************************************************/
/* STATIC                   convert_floats_to_TFLOAT                         */
/*****************************************************************************/

static void
convert_floats_to_TFLOAT(kdu_sample32 *src, float *dest, int num, 
    double minval, double maxval, short domain, 
    std::ofstream& before, std::ofstream& after)
{
  float scale, factor, offset=0.0;
  if (src == NULL)
  {kdu_error e; e << "16 bit irreversible compression is unimplemented";}

  /* The images captured in radio astronomy have extremely dynamic range of
   * values. As such a linear scaling will often result an over compressed 
   * image, because most of the data will end up being extremely close 
   * together. As such we use a log or sqrt domain to minimize the loss in 
   * precision */
  if (domain == 0) { // invert the log transform
    factor = 500;
    scale = log((maxval - minval) * factor + 1);
  }
  else if (domain == 1) { // invert the sqrt transform
    scale = sqrt(fabs(maxval-minval));
  }
  else { // invert the linear transform

  }
  offset = minval;   

  for (int i = 0; i < num; i++)
  {
    float fval = (double)((src[i].fval + 0.5) * scale);
    if (domain == 0) {
      fval = (exp(fval) - 1) / factor + offset;
    }
    else if (domain == 1) {
      fval = fval * fval + offset; 
    }
    else {
      fval += offset;    
    }

    fval = (fval > minval)?fval:minval;
    fval = (fval < maxval)?fval:maxval;
    dest[i]= fval;

    if (before != NULL) {
      before << src[i].fval << " ";
      after << dest[i] << " ";
    }
  }

  if (before != NULL) {
    before << std::endl;
    after << std::endl;
  }
}

/*****************************************************************************/
/* STATIC                    convert_ints_to_TFLOAT                          */
/*****************************************************************************/

static void
convert_ints_to_TFLOAT(kdu_line_buf &line, float *dest, int num, 
    int precision, double minval, double maxval,
    std::ofstream& before, std::ofstream& after)
{
  double scale = 1.0 / fabs(maxval - minval);
  double offset_jpx = -0.5;
  double offset_h5 = minval;

  scale *= (double)((1<<precision)-1);
  offset_jpx *= (double)(1<<precision);

  if (line.get_buf16() != NULL) {
    kdu_sample16 *src = line.get_buf16();
    for (int i = 0; i < num; i++)
    {
      double fval = (double)((src[i].ival - offset_jpx) / scale + offset_h5);
      fval = (fval > minval)?fval:minval;
      fval = (fval < maxval)?fval:maxval;
      dest[i] = fval;
      if (before != NULL) {
        before << src[i].ival << " ";
        after << dest[i] << " ";
      }
    }
  }
  else if (line.get_buf32() != NULL) {
    kdu_sample32 *src = line.get_buf32();
    for (int i = 0; i < num; i++)
    {
      double fval = (double)((src[i].ival - offset_jpx) / scale + offset_h5);
      fval = (fval > minval)?fval:minval;
      fval = (fval < maxval)?fval:maxval;
      dest[i]= fval;
      if (before != NULL) {
        before << src[i].ival << " ";
        after << dest[i] << " ";
      }
    }
  }

  if (before != NULL) {
    before << std::endl;
    after << std::endl;
  }
}

/*****************************************************************************/
/* STATIC                         str_split                                  */
/*****************************************************************************/

static std::vector<std::string> 
&str_split(const std::string &s, char delim, std::vector<std::string> &elems) 
{
  std::stringstream ss(s);
  std::string item;
  while(std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

static std::vector<std::string> 
str_split(const std::string &s, char delim) 
{
  std::vector<std::string> elems;
  return str_split(s, delim, elems);
}

/* ========================================================================= */
/*                                 hdf5_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                           hdf5_out::read_header                           */
/*****************************************************************************/

void
hdf5_out::write_header(jp2_family_tgt& tgt, kdu_args& args,
    ska_dest_file* const dest_file)
{
  kdu_error e;
  kdu_warning w;
  dest_file->num_unwritten_rows = 0;

  // default min and max
  dest_file->samples_min = -0.5;
  dest_file->samples_max = 0.5;

  // Parse hdf5 specific metadata within the JPX file.
  parse_hdf5_metadata(dims, quiet);

  if (!parse_hdf5_parameters(args, dims)) 
    kdu_error e; e << "Unable to parse HDF5 parameters";

  // Find max image components
  first_comp_idx = next_comp_idx;
  num_components = dims.get_num_components() - first_comp_idx;
  if (num_components <= 0)
    e << "Output image files require more image components "
      "(or mapped colour channels) than are available!";

  t_class = H5T_FLOAT;  // ICRAR's hdf5 dataset is stored as a float
  cinfo.naxis = 3; // ICRAR's hdf5 dataset currently has 3 dimensions
  cinfo.height = dims.get_height(first_comp_idx); // rows
  cinfo.width = dims.get_width(first_comp_idx); // cols
  cinfo.depth = num_components; // We use components currently as frames

  if (dest_file->forced_prec != -1)
    precision = forced_prec;
  else
    dest_file->precision = 32;

  // Find the sample bytes
  if (precision <= 8)
    dest_file->bytes_per_sample = 1;
  else if (precision <= 16)
    dest_file->bytes_per_sample = 2;
  else if (precision <= 32)
    dest_file->bytes_per_sample = 4;
  else 
    e << "Cannot write the output with sample precision "
      "in excess of 32 bits per sample. You may like to use the \"-fprec"
      "\" option to force the writing to a different precision."; 


  /* Setup the variables related to the output HDF5 image */

  orig_dims = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
  orig_dims[0] = cinfo.width;
  orig_dims[1] = cinfo.height;
  orig_dims[2] = cinfo.depth;

  std::cout << "JPX image dimensions:\n"
    << "rows = " << cinfo.height << "\n"
    << "cols = " << cinfo.width << "\n"
    << "frames = " << cinfo.depth << "\n";

  // TODO: implement specifying cropping
  dest_dims = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
  dest_dims[0] = cinfo.depth;
  dest_dims[1] = cinfo.height;
  dest_dims[2] = cinfo.width;

  // Create the dataspace
  dataspace = H5Screate_simple(cinfo.naxis, dest_dims, NULL); 
  if (dataspace < 0)
    e << "Unable to create dataspace for output HDF5 image.";

  // Create the new file
  file = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
  if (file < 0)
    e << "Unable to create output HDF5 image file.";

  // Create the properties in order to create the dataset
  cparms = H5Pcreate(H5P_DATASET_CREATE);
  if (cparms < 0)
    e << "Unable to create dataset properties for output "
      "HDF5 image."; 
  if (H5Pset_chunk(cparms, cinfo.naxis, dims_mem) < 0)
    e << "Unable to set chunk for dataset."; 
  int fill_value = 0;
  if (H5Pset_fill_value(cparms, H5T_NATIVE_FLOAT, &fill_value) < 0)
    e << "Unable to set fill value for dataset."; 

  // Create the dataset
  dataset = H5Dcreate2(file, DATASET_NAME, H5T_NATIVE_FLOAT, dataspace,
      H5P_DEFAULT, cparms, H5P_DEFAULT);
  if (dataset < 0)
    e << "Unable to create dataset for output HDF5 image."; 

  // Set the extent of the dataset
  if (H5Dset_extent(dataset, dest_dims) < 0)
    kdu_error e; e << "Unable to set extent of dataset.";

  // Get the filespace
  filespace = H5Dget_space(dataset);
  if (filespace < 0)
    e << "Unable to get filespace for output HDF5 image."; 

  // Create the memory space to use in put
  memspace = H5Screate_simple(cinfo.naxis, dims_mem, NULL);
  if (memspace < 0)
    e << "Unable to create memory space for HDF5 image."; 

  offset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
  offset[0] = offset[1] = offset[2] = 0;

  num_unwritten_rows = cinfo.height;
}

/*****************************************************************************/
/*                            hdf5_out::~hdf5_out                            */
/*****************************************************************************/

hdf5_out::~hdf5_out()
{
  kdu_warning w;
  kdu_error e;

  if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
    w << "Not all rows of the image component " << 
    first_comp_idx << " were completed!";

    image_line_buf *tmp;
    while ((tmp=incomplete_lines) != NULL)
    { incomplete_lines = tmp->next; delete tmp; }
    while ((tmp=free_lines) != NULL)
    { free_lines = tmp->next; delete tmp; }

    free(offset);
    free(dims_mem);
    free(orig_dims);
    free(dest_dims);

    if (H5Dclose(dataset) < 0 || H5Sclose(dataspace) < 0 || H5Fclose(file) < 0)
    e << "Unable to cleanly close HDF5 file.";
}

/*****************************************************************************/
/*                         hdf5_out::write_stripe                            */
/*****************************************************************************/

void
hdf5_out::write_stripe(int height, kdu_byte *buf, 
    ska_dest_file* const dest_file)
{
  kdu_warning w;
  kdu_error e;

  if (num_unwritten_rows == 0)
    e << "Attempting to write too many lines to image.";

  // Dimensions of hyperslab selection will be row by row
  // Also used aas chunk dimensions
  dims_mem = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
  dims_mem[0] = 1;
  dims_mem[1] = height;
  dims_mem[2] = cinfo.width;

  // Select the hyperslab (in this case row) that we are going to write to
  if (H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL,
        dims_mem, NULL) < 0)
    e << "Unable to select hyperslab within HDF5 dataset."; 

  buf = malloc(4 * width); 
  if (dest_file->reversible) {
    if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
          H5P_DEFAULT, buf) < 0)
      e << "Unable to write to HDF5 file."; 
  }
  else {
    if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
          H5P_DEFAULT, buf) < 0)
      e << "Unable to write to HDF5 file."; 
  }
  free(buf);

  offset[1] += height;

  num_unwritten_rows--;
}

/*****************************************************************************/
/*                     hdf5_out::parse_hdf5_parameters                        */
/*****************************************************************************/

bool 
hdf5_out::parse_hdf5_parameters(jp2_family_tgt &tgt, kdu_args &args) 
{
  kdu_error e;
  const char* string;

  if (args.get_first() != NULL) {
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
        if (!succ || (i == 0 && (sscanf(string, "%f", &samples_min) != 1)))
          succ = false;
        else if (!succ || (i == 1 && 
              (sscanf(string, "%f", &samples_max) != 1)))
          succ = false;

        if (!succ)
         e << "\"-minmax\" argument contains "
          "malformed specification. Expected to find two comma-" 
          "separated float numbers, enclosed by curly braces. "
          "Example: -minmax {-1.0,1.0}";
      }
      args.advance();
    }
    else {
      kdu_warning w; w << "Using default float max/min values (%f, %f). "
        "Distortion is likely to occur, it is recommended to rather specify"
        " these values using the \"-minmax\" argument";
      samples_min = H5_FLOAT_MIN;
      samples_max = H5_FLOAT_MAX;
    }

  }
  return true;
}

/*****************************************************************************/
/*                     hdf5_out::parse_hdf5_metadata                         */
/*****************************************************************************/

void 
hdf5_out::parse_hdf5_metadata(jp2_family_tgt &tgt, bool quiet)
{
  // Current structure of metadata is one box, with a comma-seperated
  // dictionary.
  // minfloat
  // maxfloat
  // start_frame
  // end_frame
  // start_stoke
  // end_stoke

  jpx_meta_manager meta_manager = dims.get_meta_manager();
  jp2_input_box h5_box;
  if (meta_manager.exists()) {
    kdu_byte uuid[16];
    kdu_byte h5_uuid[16] = {0x72,0xF7,0x1C,0x30,
      0x70,0x09,0x11,0xE2,
      0xBC,0xFD,0x08,0x00,
      0x20,0x0C,0x9A,0x66};

    jpx_metanode scn;
    jpx_metanode mn = meta_manager.access_root();
    int cnt;
    jp2_family_src *jsrc;
    for (cnt=0; (scn=mn.get_descendant(cnt)).exists(); ++cnt) {
      if (scn.get_uuid(uuid) && (memcmp(uuid, h5_uuid, 16) == 0)) {
        // Found h5 box
        jp2_locator loc = scn.get_existing(jsrc);
        h5_box.open(jsrc, loc);
        h5_box.seek(16); // Seek over the UUID
        break;
      }
    }

    if (h5_box.exists()) {
      kdu_uint32 contents_length = (kdu_uint32)
        (h5_box.get_box_bytes() - 24);
      // Header is always 24 bytes including length field
      kdu_byte *h5_data_packet = new kdu_byte[contents_length];
      h5_box.read(h5_data_packet, contents_length);

      // Do stuff with the retrieved metadata
      std::string str;
      for (int i = 0; i < contents_length; ++i)
        str += h5_data_packet[i];

      // parse data packet contents
      std::vector<std::string> dictionary = str_split(str, ',');
      for (int i = 0; i < dictionary.size(); ++i) {
        std::vector<std::string> entry = str_split(dictionary[i], ':');
        if (entry.size() == 2) {
          if (float_minvals == -11024 && entry[0] == "minfloat") {
            float_minvals = atof(entry[1].c_str());
            std::cout << "Floating point minimum: " << 
              float_minvals << " (chosen from "
              "metadata)" << std::endl;
          }
          else if (float_maxvals == -11024 && entry[0] == "maxfloat") {
            float_maxvals = atof(entry[1].c_str());
            std::cout << "Floating point maximum: " << 
              float_maxvals << " (chosen from "
              "metadata)" << std::endl;
          }
        }
      }

      delete[] h5_data_packet;
    }

    h5_box.close();
  }
} 
