Authors: Slava Kitaeff, Sean Peters
Contact: sean.peters.au@gmail.com
Feel free to contact me for any questions.

This software extends the kakadu kdu_compress and kdu_expand app to work for FITS and HDF5 files.
The software has not been tested to a complete extent.

Usage
=====

make                    - To compile
./skuareview-encode     - To encode .fits files to JPEG2000
./skuareview-decode     - To decode .jp2 or .jpx files to .fits

Running the above programs provides a more than adequate enough description on
how to execute the program.

This software runs very similarly to the kdu_buffered_compress and
kdu_buffered_expand apps available in the Kakadu Library. Kakadu Library
provides extensive documentation on example uses of these programs. 

Additional parameters available:

-icrop {x,y,z,width,height,depth}
Specifies the subcube you wish to encode to JPEG2000.

-minmax {min,max}
Directly specifies the minimum and maximum voxel value in the input cube.

NOTE: Casa is completely unimplemented. HDF5 has been implemented but has not
been tested for several months over which many updates were made to other
elements in the software - i.e. it likely does not work anymore. FITS encoding
and decoding is the only thoroughly tested file format tested for the optimized
buffered Kakadu encoding and decoding.

File details
============

kakadu_apps/kdu_buffered_compress.cpp
    Based off kdu_buffered_compress.cpp found within the Kakadu Library apps
    directory. This edited file makes use of the generic ska_source and ska_dest
    image types as opposed to Kakadu`s PGM format.
    Additionally I have added functionality for 3D image cubes.
kakadu_apps/kdu_buffered_expand.cpp
    Same as above except for decoding JPEG2000 images instead of encoding.

ska_local.h
    Header file with declarations for an encoder and decoder of a generic file
    format.
ska_dest.cpp
    Defines the generic decoder functions described above.
ska_source.cpp
    Defines the generic encoder functions described above.

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

sample_converter.h
    Declarations of helper methods used in both encoders/decoders, which are
    implemented within the kakadu apps (image_in.cpp and image_out.cpp).
sample_converter.cpp
    Defines the above methods. 

makefile
    Compiles skuareview-encode and skuareview-decode, which are just extended
    versions of kdu_compress and kdu_expand.

find_minmax.c
    A seperate tool used within the Skuareview, to find the min/max
    floating points values within a 3D HDF5 cube.

Compilation instructions are specified within the makefile.

This is currently a very brief instruction on what modificaitons WERE 
made to the Kakadu Library and Kakadu Apps in order for SkuareView-NGAS-plugin
to compile. They assume an understanding of the software.
==============================================================================

Preferably all these changes should be enclosed in #ifdef SKA

kdu_image.h
    Add another constructor declaration for kdu_image_in that includes a kdu_args parameter.
    Add another constructor declaration for kdu_image_out that includes a kdu_args parameter.
    You also need to include kdu_args.h for this to work.
image_in.cpp
    Add the definition for the declaration you made in kdu_image.h (identical to the other constructor, except including fits and hdf5).
    Both of the below changes should be contained within and #ifdef statement.
        Add includes for your local image format headers e.g. fits_local.h
        Add include for kdu_args.h
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
