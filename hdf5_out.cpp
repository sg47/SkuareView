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
/*                             hdf5_out::hdf5_out                            */
/*****************************************************************************/

hdf5_out::hdf5_out(const char *fname, kdu_args& args, kdu_image_dims &dims, int &next_comp_idx,
                   bool quiet)
{
    // Iinitialize state information in case we have to clean up prematurely
    orig_precision = NULL;
    is_signed = NULL;
    incomplete_lines = NULL;
    free_lines = NULL;
    num_unwritten_rows = 0;
    initial_non_empty_tiles = 0;
    float_minvals = float_maxvals = -11024; //Used as a value that flags, nothing in metadata
    domain=2; // Linear by default

    if (!parse_hdf5_parameters(args, dims)) 
        { kdu_error e; e << "Unable to parse HDF5 parameters"; }

    /* Retrieve and use variables related to the input JPX image */

    // Parse hdf5 specific metadata within the JPX file.
    parse_hdf5_metadata(dims, quiet);

    // Find max image components
    first_comp_idx = next_comp_idx;
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

    image_line_buf *scan=NULL, *prev=NULL;
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
        
        // Finall we write the row to HDF5 file
        float* buf = (float*) malloc(4 * width); //TODO: With other formats this
            // buffer size will change.
        if (line.is_absolute()) {
            convert_ints_to_TFLOAT(line, buf, width,
                    orig_precision[idx], float_minvals, float_maxvals,
                    raw_before, raw_after);

            if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
                         H5P_DEFAULT, buf) < 0)
                { kdu_error e; e << "Unable to write to HDF5 file."; }
        }
        else {
            convert_floats_to_TFLOAT(line.get_buf32(), buf, width,
                    float_minvals, float_maxvals, domain, raw_before,
                    raw_after);

            if (H5Dwrite(dataset, H5T_NATIVE_FLOAT, memspace, filespace,
                         H5P_DEFAULT, buf) < 0)
                { kdu_error e; e << "Unable to write to HDF5 file."; }
        }
        free(buf);

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

/*****************************************************************************/
/*                     hdf5_out::parse_hdf5_parameters                        */
/*****************************************************************************/

bool 
hdf5_out::parse_hdf5_parameters(kdu_args &args, kdu_image_dims &dims) 
{
    const char* string;

    if (args.get_first() != NULL) {
        /* Ouput the values decoded before and after renormalization to a raw
         * data file for testing analysis*/
        if (args.find("-rawtest") != NULL)
        {
            /* raw data values before they are normalized for the JPX image. These
             * can be compared against decoder_after_raw, to see how the precision
             * of the values compared after they have been renormalized back to
             * they're origional values. */
            raw_before.open("decoder_before_rawtest");

            /* raw data values after they are normalized for the JPX image. These
             * can be compared against the decoder_before_raw, to see how the 
             * precision of the values is affected by the internal Kakadu compressor
             * exclusively. */
            raw_after.open("decoder_after_rawtest");
            args.advance();
        }
        
        if (args.find("-domain") != NULL)
        {
            const char *string = args.advance();
            if (strcmp(string, "log") == 0)
                domain=0;
            else if (strcmp(string, "sqrt") == 0)
                domain=1;
            else
                domain=2; // linear
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
           kdu_warning w; w << "Using default float max/min values (%f, %f). "
           "Distortion is likely to occur, it is recommended to rather specify"
           " these values using the \"-minmax\" argument";
           float_minvals = H5_FLOAT_MIN;
           float_maxvals = H5_FLOAT_MAX;
        }

    }
    return true;
}

/*****************************************************************************/
/*                     hdf5_out::parse_hdf5_metadata                         */
/*****************************************************************************/

void 
hdf5_out::parse_hdf5_metadata(kdu_image_dims &dims, bool quiet)
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
                    // Much of the metadata may never be used in encoding
                    // or decoding, but helps just to describe context 
                    // specific information on where images may have come
                    // from.
                }
            }

            delete[] h5_data_packet;
        }

        h5_box.close();
    }
} 
