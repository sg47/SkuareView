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

COMPILER=g++ -g -DSKA#-pg

OBJS=args.o jp2.o
E_OBJS=$(OBJS) hdf5_in.o kdu_stripe_compressor.o
#fits_in.o 
#D_OBJS=$(OBJS) fits_out.o hdf5_out.o kdu_stripe_decompressor.o

# Directory absolute paths
APPS=/home/speters/kakadu-v7/v7_2_1-01265L/apps
COMPRESS=$(APPS)/kdu_buffered_compress/kdu_buffered_compress.cpp
EXPAND=$(APPS)/kdu_buffered_expand/kdu_buffered_expand.cpp
SUPPORT=$(APPS)/support

# Libraries
KDU_LIBS=-lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu_aux
FITS_LIB=-lcfitsio
HDF5_LIBS=-lhdf5 -lhdf5_hl -lz -lsz
CASA_LIBS=
TIFF_LIBS=-ltiff
LIBS=$(KDU_LIBS) $(FITS_LIB) $(HDF5_LIBS) $(CASA_LIBS) $(TIFF_LIBS)

all: $(ENC) 
#$(DEC)

$(ENC): $(COMPRESS) $(E_OBJS)
	$(COMPILER) $(COMPRESS) -o $(ENC) $(E_OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

#$(DEC): $(EXPAND) $(D_OBJS)
#$(COMPILER) $(EXPAND) -o $(DEC) $(D_OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

hdf5_in.o: hdf5_in.cpp 
	$(COMPILER) -c hdf5_in.cpp $(LIBS)

#hdf5_out.o: hdf5_out.cpp 
#$(COMPILER) -c hdf5_out.cpp $(LIBS)

#fits_in.o: fits_in.cpp 
#$(COMPILER) -c fits_in.cpp $(LIBS) 

#fits_out.o: fits_out.cpp
#$(COMPILER) -c fits_out.cpp $(LIBS)

jp2.o: $(APPS)/jp2/jp2.cpp
	$(COMPILER) -c $(APPS)/jp2/jp2.cpp -o jp2.o

args.o: $(APPS)/args/args.cpp
	$(COMPILER) -c $(APPS)/args/args.cpp -o args.o

#kdu_stripe_decompressor.o: $(SUPPORT)/kdu_stripe_decompressor.cpp
#$(COMPILE) -c $(SUPPORT)/kdu_stripe_decompressor.cpp -o kdu_stripe_decompressor.o

kdu_stripe_compressor.o: $(SUPPORT)/kdu_stripe_compressor.cpp
	$(COMPILER) -c $(SUPPORT)/kdu_stripe_compressor.cpp -o kdu_stripe_compressor.o

clean:
	rm -rf *.o skuareview-*
