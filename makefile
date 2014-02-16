# @author Sean Peters
# @brief Makefile for SkuareView-NGAS-plugin
# Compilation Instruction:
# To compile, you will need to specify a path to the apps provided by the kakadu library.
# The Libraries required are listed under "# Libraries"
# If you are working with a newly compiled version of kakadu. Some edits will be required,
# these are specified within the README.
# Had to edit kdu_compress.cpp image_in.cpp kdu_image.h (in apps and all includes),
# changes are tagged with SkuareView-plugin

ENC=skuareview-encode
DEC=skuareview-decode

COMPILER=g++ -g #-pg

E_OBJS=image_in.o fits_in.o hdf5_in.o roi_sources.o palette.o
D_OBJS=image_out.o hdf5_out.o palette.o
OBJS=$(E_OBJS) $(D_OBJS)

# Directory absolute paths
APPS=v7_2-01265L/apps
COMPRESS=$(APPS)/kdu_compress
EXPAND=$(APPS)/kdu_expand
IMAGE=$(APPS)/image

# Libraries
KDU_LIBS=-lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu_aux # just the kakadu library
FITS_LIB=-lcfitsio
HDF5_LIBS=-lhdf5 -lhdf5_hl -lz -lsz
CASA_LIBS=
TIFF_LIBS=-ltiff
LIBS=$(KDU_LIBS) $(FITS_LIB) $(HDF5_LIBS) $(CASA_LIBS) $(TIFF_LIBS)

all: $(ENC) $(DEC)

$(ENC): $(COMPRESS)/kdu_compress.cpp $(E_OBJS)
	$(COMPILER) $(COMPRESS)/kdu_compress.cpp -o $(ENC) $(E_OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

$(DEC): $(EXPAND)/kdu_expand.cpp $(D_OBJS)
	$(COMPILER) $(EXPAND)/kdu_expand.cpp -o $(DEC) $(D_OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

image_in.o: $(IMAGE)/image_in.cpp 
	$(COMPILER) -c $(IMAGE)/image_in.cpp $(LIBS) -DSKA_IMG_FORMATS=1 -DKDU_INCLUDE_TIFF=1

image_out.o: $(IMAGE)/image_out.cpp
	$(COMPILER) -c $(IMAGE)/image_out.cpp $(KDU_LIBS) -DSKA_IMG_FORMATS=1

hdf5_in.o: hdf5_in.cpp 
	$(COMPILER) -c hdf5_in.cpp $(LIBS)

hdf5_out.o: hdf5_out.cpp 
	$(COMPILER) -c hdf5_out.cpp $(LIBS)

fits_in.o: fits_in.cpp 
	$(COMPILER) -c fits_in.cpp $(LIBS) 

roi_sources.o: $(APPS)/kdu_compress/roi_sources.cpp
	$(COMPILER) -c $(COMPRESS)/roi_sources.cpp $(KDU_LIBS)

palette.o: $(APPS)/image/palette.cpp
	$(COMPILER) -c $(IMAGE)/palette.cpp 

clean:
	rm -rf *.o skuareview-*
