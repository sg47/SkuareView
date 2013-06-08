// STL includes
#include <iostream>
#include <cmath>
#include <string>
// HDF5 includes
#include "hdf5.h"
// FITS includes
#include "fitsio.h"

void print_usage() {
  std::cout << "Usage: " << std::endl
    << "./h2f [hdf5 file] [fits file] [x] [y] [z] [width] [height] [depth]" 
    << std::endl 
    << "Remember that the cube's x dimension represents frequency!"
    << std::endl;
}

void err(std::string message) {
  std::cerr << "ERROR: " << message << std::endl;
  exit(2);
}

int main (int argc, char*argv[]) {
  /* HDF5 file must follow these specifications: 
   *    - 32 bit floating point samples
   *    - dataset name must be "full_cube"        
   */
  char* hname;
  char* fname;
  hsize_t* crop_offset = new hsize_t [3];
  long* crop_extent = new long [3]; 
  if (argc == 9) {
    hname = argv[1];

    char* temp_fname = argv[2];
    fname = new char [BUFSIZ];
    fname[0] = '!'; // this character lets the cfitsio library know to
      //overwrite any existing file
    int i;
    for(i = 0; temp_fname[i] != '\0'; ++i)
      fname[i+1] = temp_fname[i];

    crop_offset[0] = atoi(argv[3]);
    crop_offset[1] = atoi(argv[4]);
    crop_offset[2] = atoi(argv[5]);
    crop_extent[0] = atoi(argv[6]);
    crop_extent[1] = atoi(argv[7]);
    crop_extent[2] = atoi(argv[8]);
  }
  else {
    print_usage();
    return 0;
  }

  /* Setup for HDF5 decoding */
  hid_t hfile = H5Fopen(hname, H5F_ACC_RDONLY, H5P_DEFAULT);
  if (hfile < 0) 
    err("Unable to open input HDF5 file.");
  hid_t dataset = H5Dopen(hfile, "full_cube", H5P_DEFAULT);
  if (dataset < 0) 
    err("Unable to open dataset in HDF5 file.");
  hid_t datatype = H5Dget_type(dataset);
  if (datatype < 0) 
    err("Unable to get datatype in HDF5 file.");
  int t_class = H5Tget_class(datatype);
  if (t_class == H5T_NO_CLASS) 
    err("Unable to get class in HDF5 file.");
  hid_t dataspace = H5Dget_space(dataset);
  if (dataspace < 0) 
    err("Unable to get dataspace in HDF5 file.");
  int rank = H5Sget_simple_extent_ndims(dataspace);
  if (rank < 0) 
    err("Unable to get dataspace rank in HDF5 file.");
  hsize_t* extent = new hsize_t [rank];
  if (H5Sget_simple_extent_dims(dataspace, extent, NULL) != rank)
    err("Unable to get extent of dataspace in HDF5 file.");
  // HDF5 cube (FREQ,DEC,RA). FREQ has slow access so, we incrementally grab a
  // full line of RA.
  hsize_t* mem_extent = new hsize_t [rank];
  hsize_t* mem_offset = new hsize_t [rank];
  for(int i = 0; i < rank; ++i)
    mem_offset = 0;
  for(int i = 0; i < rank-1; ++i)
    mem_extent[i] = 1;
  mem_extent[rank-1] = extent[rank-1];
  hid_t memspace = H5Screate_simple(rank, mem_extent, NULL);
  if (memspace < 0) 
    err("Unable to create memory space for HDF5 file.");

  /* Setup for FITS encoding */
  fitsfile* ffile;
  int status;
  std::cout << fname << std::endl;
  fits_create_file(&ffile, fname, &status);
  if (status != 0) 
    err("Unable to create FITS file.");
  int iomode = READWRITE;
  fits_file_mode(ffile, &iomode, &status);
  if (status != 0) 
    err("Unable to select iomode in FITS file.");
  // FITS cube (RA,DEC,FREQ)
  std::swap (crop_extent[0], crop_extent[2]);
  fits_create_img(ffile, FLOAT_IMG, rank, crop_extent, &status);
  std::swap (crop_extent[0], crop_extent[2]);
  if (status != 0) 
    err("Unable to create image in FITS file.");

  /* Perform conversion */ 
  if (rank != 3) 
    err("Only 3D image conversion implemented.");

  long* fpixel = new long[rank];
  for(int i = 0; i < rank; ++i)
    fpixel[i] = crop_offset[i];
  for(; fpixel[0] < crop_offset[0] + crop_extent[0]; ++fpixel[0]) {
    for(; fpixel[1] < crop_offset[1] + crop_extent[1]; ++fpixel[1]) {
      std::cout << mem_offset[0] << " " << mem_offset[1] << " " << mem_offset[2]
        << " " <<mem_extent[0] << " " << mem_extent[1] << " " << mem_extent[2] << std::endl;
      if (H5Sselect_hyperslab(memspace, H5S_SELECT_SET, mem_offset, NULL,
        mem_extent, NULL) < 0)
        err("Unable to select memory space for HDF5.");
      if (H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, crop_offset, NULL, 
        mem_extent, NULL) < 0)
        err("Unable to select stripe in HDF5.");
      float* stripe = new float [crop_extent[2]];
      if (H5Dread(dataset, H5T_NATIVE_FLOAT, memspace, dataspace,
            H5P_DEFAULT, stripe) < 0)
        err("Unable to read from HDF5 dataset.");
      fits_write_pix(ffile, TFLOAT, fpixel, crop_extent[2], stripe, &status);
      delete[] stripe;
    }
  }

  delete[] fpixel;
  delete[] extent;
  delete[] mem_extent;
  delete[] crop_offset;
  delete[] crop_extent;

  if (H5Tclose(datatype) < 0 ||
      H5Dclose(dataset) < 0 ||
      H5Sclose(dataspace) < 0 ||
      H5Sclose(memspace) < 0 ||
      H5Fclose(hfile) < 0)
    err("Unable to close HDF5 file.");

  fits_close_file(ffile, &status);
  if (status != 0) err("Unable to close FITS file.");

  return 0;
}
