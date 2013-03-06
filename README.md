# This software extends the kakadu kdu_compress and kdu_expand app to work for FITS and HDF5 files.
# The software has not been tested to a complete extent.

# NOTE: The HDF5 implemented to convert ICRARs HDF5 image format has precision problems with large images.
# A solution to this is being researched at this time (February 2013).

# File details

fits_local.h
    Header file with declarations for fits_in.cpp and fits_out.cpp
fits_in.cpp
    Defines the classes and methods for encoding a FITS image to JPEG2000
fits_out.cpp
    Defines the classes and methods for decoding a FITS image from JPEG2000
hdf5_local.h
    Header file with declarations for hdf5_in.cpp and hdf5_out.cpp
hdf5_in.cpp
    Defines the classes and methods for encoding an HDF5 image to JPEG2000
hdf5_out.cpp
    Defines the classes and methods for decoding an HDF5 image from JPEG2000
image_local_helpers.h
    Declarations of helper methods used in both encoders/decoders, which are
    implemented within the kakadu apps (image_in.cpp and image_out.cpp).
makefile
    Compiles skuareview-encode and skuareview-decode, which are just extended
    versions of kdu_compress and kdu_expand.
find_minmax.c
    A seperate tool used within the Skuareview-NGAS-plugin, to find the min/max
    floating points values within a 3D HDF5 cube.


# Compilation instructions are specified within the makefile.

# This is currently a very brief instruction on what modificaitons need to be
# made to the Kakadu Library and Kakadu Apps in order for SkuareView-NGAS-plugin
# to compile. They assume an understanding of the software.

kdu_image.h
    Add another constructor declaration for kdu_image_in that includes a kdu_args parameter.
    Add another constructor declaration for kdu_image_out that includes a kdu_args parameter.
    You also need to include kdu_args.h for this to work.
image_in.cpp
    Add the definition for the declaration you made in kdu_image.h (identical to the other constructor, except including fits and hdf5).
    Both of the below changes should be contained within and #ifdef statement.
        Add includes for your local image format headers e.g. fits_local.h
        Modify kdu_image_in constructor definition to include your image types.
image_out.cpp
    Same as image_in.cpp 
kdu_compress.cpp
    Add args parameter when initializing kdu_image_in classes (encloded in #ifdef for our project).
kdu_expand.cpp
    Add args parameter when initializing kdu_image_out classes (enclosed in #ifdef for our project).
    You will also need to move the line "args.show_unrecognized..." to after the #ifdef
image_local_helpers.h
    All the methods declared in this header are defined within image_in.cpp (possiblity image_out.cpp),
    they will be static in a clean version of kakadu. The static keyword needs to be removed, before
    all these functions.
