# @author Sean Peters
# @brief Makefile for SkuareView-NGAS-plugin
# TODO: Compilation instructions
# Had to edit kdu_compress.cpp image_in.cpp kdu_image.h (in apps and all includes),
# changes are tagged with SkuareView-plugin

PROJ=skuareview-plugin
OBJS=image_in.o fits_in.o hdf5_in.o roi_sources.o palette.o

# Directory absolute paths
APPS=/home/speters/kakadu-v7/v7_2-01265L/apps
COMPRESS=$(APPS)/kdu_compress
IMAGE=$(APPS)/image

# Libraries
KDU_LIBS=-lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu_aux
FITS_LIB=-lcfitsio
HDF5_LIBS=-lhdf5 -lhdf5_hl -lz -lsz
CASA_LIBS=
LIBS=$(KDU_LIBS) $(FITS_LIB) $(HDF5_LIBS) $(CASA_LIBS)

$(PROJ): $(COMPRESS)/kdu_compress.cpp $(OBJS)
	g++ $(COMPRESS)/kdu_compress.cpp -o $(PROJ) $(OBJS) $(LIBS) -DSKA_IMG_FORMATS=1

image_in.o: $(APPS)/image/image_in.cpp 
	g++ -c $(IMAGE)/image_in.cpp $(KDU_LIBS) -DSKA_IMG_FORMATS=1

hdf5_in.o: hdf5_in.cpp 
	g++ -c hdf5_in.cpp $(LIBS)

fits_in.o: fits_in.cpp 
	g++ -c fits_in.cpp $(LIBS) 

roi_sources.o: $(APPS)/kdu_compress/roi_sources.cpp
	g++ -c $(COMPRESS)/roi_sources.cpp $(KDU_LIBS)

palette.o: $(APPS)/image/palette.cpp
	g++ -c $(IMAGE)/palette.cpp 

clean:
	rm -rf *.o fits_to_j2k
