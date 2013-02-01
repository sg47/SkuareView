# @author Sean Peters
# @brief Makefile for SkuareView-NGAS-plugin
# TODO: Compilation instructions
# Had to edit kdu_compress.cpp image_in.cpp kdu_image.h (in apps and all includes),
# changes are tagged with SkuareView-plugin

ENC=skuareview-encode
DEC=skuareview-decode

E_OBJS=image_in.o fits_in.o hdf5_in.o roi_sources.o palette.o
D_OBJS=image_out.o hdf5_out.o palette.o
OBJS=$(E_OBJS) $(D_OBJS)

# Directory absolute paths
APPS=/home/speters/kakadu-v7/v7_2-01265L/apps
COMPRESS=$(APPS)/kdu_compress
EXPAND=$(APPS)/kdu_expand
IMAGE=$(APPS)/image

# Libraries
KDU_LIBS=-lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu_aux
FITS_LIB=-lcfitsio
HDF5_LIBS=-lhdf5 -lhdf5_hl -lz -lsz
CASA_LIBS=
LIBS=$(KDU_LIBS) $(FITS_LIB) $(HDF5_LIBS) $(CASA_LIBS)

all: $(ENC) $(DEC)

$(ENC): $(COMPRESS)/kdu_compress.cpp $(E_OBJS)
	g++ $(COMPRESS)/kdu_compress.cpp -o $(ENC) $(E_OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

$(DEC): $(EXPAND)/kdu_expand.cpp $(D_OBJS)
	g++ $(EXPAND)/kdu_expand.cpp -o $(DEC) $(D_OBJS) $(LIBS) -DKSA_IMG_FORMATS=1

image_in.o: $(IMAGE)/image_in.cpp 
	g++ -c $(IMAGE)/image_in.cpp $(KDU_LIBS) -DSKA_IMG_FORMATS=1

image_out.o: $(IMAGE)/image_out.cpp
	g++ -c $(IMAGE)/image_out.cpp $(KDU_LIBS) -DSKA_IMG_FORMATS=1

hdf5_in.o: hdf5_in.cpp 
	g++ -c hdf5_in.cpp $(LIBS)

fits_in.o: fits_in.cpp 
	g++ -c fits_in.cpp $(LIBS) 

roi_sources.o: $(APPS)/kdu_compress/roi_sources.cpp
	g++ -c $(COMPRESS)/roi_sources.cpp $(KDU_LIBS)

palette.o: $(APPS)/image/palette.cpp
	g++ -c $(IMAGE)/palette.cpp 

clean:
	rm -rf *.o skuareview-*
