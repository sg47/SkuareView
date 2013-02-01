# This is currently a very brief instruction on what modificaitons need to be
# made to the Kakadu Library and Kakadu Apps in order for SkuareView-NGAS-plugin
# to compile. They assume an understanding of the software.

kdu_image.h
    Add another constructor declaration for kdu_image_in that includes a kdu_args parameter.
    You also need to include kdu_args.h for this to work.
image_in.cpp
    Add the definition for the declaration you made in kdu_image.h
    Both of the below changes should be contained within and #ifdef statement.
        Add includes for your local image format headers e.g. fits_local.h
        Modify kdu_image_in constructor definition to include your image types.
image_out.cpp
    Same as image_in.cpp (excluding the args parameter - for now)
kdu_compress.cpp
    Add args parameter when initializing kdu_image_in classes.
kdu_expand.cpp
    Currently no changes required
