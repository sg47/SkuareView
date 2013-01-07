
APPS_PATH = /home/speters/kakadu-v7/2beta-01265L/apps/make/

fits_to_j2k: fits_to_j2k.cpp fits_image_in.o # roi_source.o pallet.o
	g++ fits_to_j2k.cpp -o fits_to_j2k fits_image_in.o $(APPS_PATH)roi_sources.o $(APPS_PATH)palette.o -lcfitsio -lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu_aux	

fits_image_in.o: fits_image_in.cpp
	g++ -c fits_image_in.cpp -lcfitsio -lkdu_v72R -lkdu_a72R -lkdu -lm -lnsl -lkdu

# Uncomment to recompile apps in kakadu
#roi_source.o pallet.o: roi_source.o
#	make $(APPS_PATH)Makefile-Linux-x86-32-gcc

clean:
	rm -rf *.o fits_to_j2k
