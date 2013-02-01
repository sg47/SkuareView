// System includes
#include <iostream>
#include <string.h>
#include <math.h>
#include <assert.h>
// Core includes
#include "kdu_messaging.h"
#include "kdu_sample_processing.h"
// Image includes
#include "kdu_image.h"
#include "image_local.h"
#include "hdf5_local.h"

/* ========================================================================= */
/*                                 hdf5_out                                  */
/* ========================================================================= */

/*****************************************************************************/
/*                             hdf5_out::hdf5_out                            */
/*****************************************************************************/

hdf5_out::hdf5_out(const char *fname, kdu_image_dims &dims, int &next_comp_idx,
                   bool quiet)
{
    // Iinitialize state information in case we have to clean up prematurely
    orig_precision = NULL;
    is_signed = NULL;
    incomplete_lines = NULL;
    free_lines = NULL;
    num_unwritten_rows = 0;
    initial_non_empty_tiles = 0;

    /* Retrieve and use variables related to the input JPX image */

    // Find max image components
    first_comp_idx = next_comp_idx;
    std::cout << dims.get_num_components() << std::endl;
    num_components = dims.get_num_components() - first_comp_idx;
    if (num_components <= 0)
        { kdu_error e; e << "Output image files require more image components "
          "(or mapped colour channels) than are available!"; }

    cinfo.t_class = H5T_FLOAT;  // ICRAR's hdf5 dataset is stored as a float
    cinfo.naxis = 3; // ICRAR's hdf5 dataset currently has 3 dimensions
    cinfo.height = dims.get_height(first_comp_idx); // rows
    cinfo.width = dims.get_width(first_comp_idx); // cols
    cinfo.depth = num_components; // We use components currently as frames

    // Find component dimensions and other info
    // As far as I know all components will always have the same is_signed
    // and precision values. However just in case we assume they do not 
    is_signed = new bool[num_components];
    orig_precision = new int[num_components];
    precision = 0; // Just for now
    forced_align_lsbs = false; // Just for now

    for (int n = 0; n < num_components; ++n, ++next_comp_idx) {
        is_signed[n] = dims.get_signed(next_comp_idx);
        int comp_prec = orig_precision[n] = dims.get_bit_depth(next_comp_idx);
        bool align_lsbs = false;
        
        // implemement the -fprec parameter
        int forced_prec = dims.get_forced_precision(next_comp_idx, align_lsbs);
        if (forced_prec != 0)
            comp_prec  = forced_prec;
        
        if (n == 0) {
            precision = comp_prec;
            forced_align_lsbs = align_lsbs;
        }
        if ((cinfo.height != dims.get_height(next_comp_idx)) ||
            (cinfo.width != dims.get_width(next_comp_idx)) ||
            (comp_prec != precision) || (forced_align_lsbs != align_lsbs)) {
            assert(n > 0);
            break;
        }
    }
    next_comp_idx = first_comp_idx + num_components;

    // Find the sample bytes
    if (precision <= 8)
        sample_bytes = 1;
    else if (precision <= 16)
        sample_bytes = 2;
    else if (precision <= 32)
        sample_bytes = 4;
    else 
        { kdu_error e; e << "Cannot write the output with sample precision "
            "in excess of 32 bits per sample. You may like to use the \"-fprec"
            "\" option to force the writing to a different precision."; }
    
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

    // Dimensions of hyperslab selection will be row by row
    // Also used aas chunk dimensions
    dims_mem = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    dims_mem[0] = 1;
    dims_mem[1] = 1;
    dims_mem[2] = cinfo.width;

    // Create the dataspace
    dataspace = H5Screate_simple(cinfo.naxis, dest_dims, NULL); 
    if (dataspace < 0)
        { kdu_error e; e << "Unable to create dataspace for output HDF5 image."; }

    // Create the new file
    file = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
    if (file < 0)
        { kdu_error e; e << "Unable to create output HDF5 image file."; }

    // Create the properties in order to create the dataset
    cparms = H5Pcreate(H5P_DATASET_CREATE);
    if (cparms < 0)
        { kdu_error e; e << "Unable to create dataset properties for output "
                            "HDF5 image."; }
    if (H5Pset_chunk(cparms, cinfo.naxis, dims_mem) < 0)
        { kdu_error e; e << "Unable to set chunk for dataset."; }
    int fill_value = 0;
    if (H5Pset_fill_value(cparms, H5T_NATIVE_FLOAT, &fill_value) < 0)
        { kdu_error e; e << "Unable to set fill value for dataset."; }

    // Create the dataset
    dataset = H5Dcreate2(file, DATASET_NAME, H5T_NATIVE_FLOAT, dataspace,
                         H5P_DEFAULT, cparms, H5P_DEFAULT);
    if (dataset < 0)
        { kdu_error e; e << "Unable to create dataset for output HDF5 image."; }

    // Set the extent of the dataset
    if (H5Dset_extent(dataset, dest_dims) < 0)
        { kdu_error e; e << "Unable to set extent of dataset."; }

    // Get the filespace
    filespace = H5Dget_space(dataset);
    if (filespace < 0)
        { kdu_error e; e << "Unable to get filespace for output HDF5 image."; }

    // Create the memory space to use in put
    memspace = H5Screate_simple(cinfo.naxis, dims_mem, NULL);
    if (memspace < 0)
        { kdu_error e; e << "Unable to create memory space for HDF5 image."; }

    offset = (hsize_t*) malloc(sizeof(hsize_t) * cinfo.naxis);
    offset[0] = offset[1] = offset[2] = 0;

    num_unwritten_rows = cinfo.height;
}

/*****************************************************************************/
/*                            hdf5_out::~hdf5_out                            */
/*****************************************************************************/

hdf5_out::~hdf5_out()
{
    if ((num_unwritten_rows > 0) || (incomplete_lines != NULL))
        { kdu_warning w; w << "Not all rows of the image component " << 
            first_comp_idx << " were completed!"; }

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
        { kdu_error e; e << "Unable to cleanly close HDF5 file."; }
}

/*****************************************************************************/
/*                               hdf5_out::put                               */
/*****************************************************************************/

void
    hdf5_out::put(int comp_idx, kdu_line_buf &line, int x_tnum)
{
    int width = line.get_width();
    int idx = comp_idx - this->first_comp_idx;

    // ICRAR's current HDF5 image format makes no use of tiles. So much of the
    // tile related code, may be considered a little superfluous and is 
    // completely untested. However I thought to include it just in case.
    x_tnum = x_tnum * num_components + idx; 

    if ((initial_non_empty_tiles != 0) && (x_tnum >= initial_non_empty_tiles)) {
        assert(width == 0);
        return;
    }

    image_line_buf *scan, *prev=NULL;
    for (scan=incomplete_lines; scan != NULL; prev=scan, scan=scan->next) {
        assert(scan->next_x_tnum >= x_tnum);
        if (scan->next_x_tnum == x_tnum)
            break;
    }

    if (scan == NULL) { // Need to open a new line buffer.
        assert(x_tnum == 0); // Must supply samples from left to right.
        if ((scan = free_lines) == NULL)
            // Big enough for padding and expanding bits to bytes
            scan = new image_line_buf(width, sample_bytes);
        free_lines = scan->next;
        if (prev == NULL)
            incomplete_lines = scan;
        else
            prev->next = scan;
        scan->accessed_samples = 0;
        scan->next_x_tnum = 0;
    }
    assert((scan->width-scan->accessed_samples) >= line.get_width());

    scan->next_x_tnum++; 
    if (idx == (num_components-1))
        scan->accessed_samples += line.get_width();
    if (scan->accessed_samples == cinfo.width) {
        // Write completed line and send it to the free list
        if (initial_non_empty_tiles == 0)
            initial_non_empty_tiles = scan->next_x_tnum;
        else
            assert(initial_non_empty_tiles == scan->next_x_tnum);

        if (num_unwritten_rows == 0)
            { kdu_error e; e << "Attempting to write too many lines to image "
              "file for components " << first_comp_idx << " through "
              << first_comp_idx + num_components - 1 << "."; }
              
        // Select the hyperslab (in this case row) that we are going to write to
        if (H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL,
                dims_mem, NULL) < 0)
            { kdu_error e; e << "Unable to select hyperslab within HDF5 dataset."; }
        
        if (line.get_buf32() != NULL) {
            float* buf = (float*) malloc(sample_bytes * width);
            kdu_sample32* tmp = line.get_buf32();
            for (int n = 0 ; n < width; ++n)
                buf[n] = tmp[n].fval;
            if (line.is_absolute()) {
                 std::cout << "unimplemented" << std::endl;
            }
            else {
                // Finall we write the row to HDF5 file
                if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
                             H5P_DEFAULT, buf) < 0)
                    { kdu_error e; e << "Unable to write to HDF5 file."; }
            }
            free(buf);
        }
        else {
            std::cout << "unimplemented" << std::endl;
        }

        // Adjust our offset in the image after writing the row
        // TODO: currently specifiying a cropping on the image is unimplemented.
        offset[0] = 0; // set col to beginning of next line
        if (offset[1] == dest_dims[1] - 1) { // just read last row in frame
            offset[1] = 0; // set row to beginning of next frame
            if (cinfo.naxis > 2 && cinfo.depth > 1) {
                if (offset[2] != dest_dims[2] - 1) {
                    offset[2]++; // next frame
                    ++comp_idx;      // new frame - next component
                }
            }
        }
        else {
            offset[1]++; // otherwise just go to next row
        }
        
        num_unwritten_rows--;
        assert(scan == incomplete_lines);
        incomplete_lines = scan->next;
        scan->next = free_lines;
        free_lines = scan;
    }
}
