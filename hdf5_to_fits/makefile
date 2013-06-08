# @AUTHOR: Sean Peters
# @BRIEF: Makefile for a converter from an HDF5 cube to FITS. The HDF5 image
# have a 3D dataset named "full_cube" with 32 bit floating point samples
# @DEPENDENCIES: cfitsio and hdf5 libraries

COMPILER=g++ -g
LIBS=-lcfitsio -lhdf5 -lhdf5_hl -lz -lsz

all: 
	$(COMPILER) hdf5_to_fits.cpp $(LIBS) -o hdf5_to_fits

clean:
	rm -f hdf5_to_fits
