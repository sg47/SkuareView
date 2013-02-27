/*
 * @author Sean Peters
 * @date 04/02/13
 * @brief Short program to determine the min and max floats in a HDF5 float cube
 */

#include "stdlib.h"
#include "hdf5.h"
#include "string.h"
#define H5FILE_NAME        "/mnt/science/DINGO/skynet_cubelets/ed_merged_cube.h5"
#define DATASETNAME 	   "full_cube"
#define RANK         3

	int
main (int argc, char** argv)
{
	// Parse script parameters	
	int XO = atoi(argv[1]);
	int YO = atoi(argv[2]);
	int ZO = atoi(argv[3]);

	int XC = atoi(argv[4]);
	int YC = atoi(argv[5]);
	int ZC = atoi(argv[6]);

	int          i, j, k, y, x, status_n, rank;

	int ferrors = 0; // file
	int derrors = 0; // dataset
	int serrors = 0; // dataspace
	int merrors = 0; // mempsace
	int mherrors = 0; // memspace hyperslab
	int sherrors = 0; // dataspace hyperslab
	int rerrors = 0; // read
	int cerrors = 0; // close

	float min = 1024;
	float max = -1024;

	hid_t       file, dataset;         /* handles */
	hid_t       datatype, dataspace;
	hid_t       memspace;
	H5T_class_t t_class;               /* data type class */

	hsize_t     dimsm[3];              /* memory space dimensions */
	herr_t      status;

	float       data_out[ZC]; /* output buffer */
	memset (data_out, 0, ZC);

	hsize_t      count[3];              /* size of the hyperslab in the file */
	hsize_t      offset[3];             /* hyperslab offset in the file */
	hsize_t      count_out[3];          /* size of the hyperslab in memory */
	hsize_t      offset_out[3];         /* hyperslab offset in memory */

	/*
	 * Open the file and the dataset.
	 */
	file = -1;
	while (file < 0) {
		file = H5Fopen(H5FILE_NAME, H5F_ACC_RDONLY, H5P_DEFAULT);
		if (file < 0)
			ferrors++;
	}

	dataset = -1;
	while (dataset < 0) {
		dataset = H5Dopen2(file, DATASETNAME, H5P_DEFAULT);
		if (dataset < 0)
			derrors++;
	}

	/*
	 * Get datatype and dataspace handles and then query
	 * dataset class, order, size, rank and dimensions.
	 */

	dataspace = -1;
	while (dataspace < 0) {
		dataspace = H5Dget_space(dataset);    /* dataspace handle */
		if (dataspace < 0)
			serrors++;
	}

	/*
	 * Define the memory dataspace.
	 */
	// This must fit within the dimensions of the dataspace hyperslab
	dimsm[0] = 1;
	dimsm[1] = 1;
	dimsm[2] = ZC;
	memspace = -1;
	while (memspace < 0) {
		memspace = H5Screate_simple(RANK,dimsm,NULL);
		if (memspace < 0)
			merrors++;
	}

	/*
	 * Define memory hyperslab.
	 */
	// This must fit within the dimensions of memspace
	offset_out[0] = 0;
	offset_out[1] = 0;
	offset_out[2] = 0;
	count_out[0]  = 1;
	count_out[1]  = 1;
	count_out[2]  = ZC;

	while (H5Sselect_hyperslab(memspace, H5S_SELECT_SET, offset_out, NULL,
				count_out, NULL) < 0) {
		mherrors++;
	}

	for (x = 0; x < XC; ++x) {
        float plane_min = 1024;
        float plane_max = -1024;

		for (y = 0; y < YC; ++y) {
			offset[0] = XO + x;
			offset[1] = YO + y;
			offset[2] = ZO + 0;
			count[0]  = 1; 
			count[1]  = 1; 
			count[2]  = ZC;

			while (H5Sselect_hyperslab(dataspace, H5S_SELECT_SET, offset, NULL,
						count, NULL) < 0) {
				sherrors++;
			}

			/*
			 * Read data from hyperslab in the file into the hyperslab in
			 * memory and display.
			 */

			while (status_n = H5Dread(dataset, H5T_NATIVE_FLOAT, memspace, dataspace,
						H5P_DEFAULT, data_out) < 0) {
				cerrors++;
			}

			for (j = 0; j < ZC; j++) {
				if (data_out[j] > max)
					max = data_out[j];
				if (data_out[j] < min)
					min = data_out[j];
				if (data_out[j] > plane_max)
					plane_max = data_out[j];
				if (data_out[j] < plane_min)
					plane_min = data_out[j];
			}

		}
        printf("%d,%f,%f\n", XO + x, plane_min, plane_max);
	}

	// The -1 is just to make it easier for a program to parse this after
 	printf("-1,TOTAL: %f, %f\n", min, max);

	/*
	 * Close/release resources.
	 */
	while (H5Dclose(dataset) < 0)
		cerrors++;
	while (H5Sclose(dataspace) < 0)
		cerrors++;
	while (H5Sclose(memspace) < 0)
		cerrors++;
	while (H5Fclose(file) < 0)
		cerrors++;
	return 0;
}
