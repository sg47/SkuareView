clean:
	rm -f *.o *.so *.a *.dll

$(STATIC_LIB_NAME) :: block_coding_common.o block_decoder.o block_encoder.o decoder.o encoder.o mq_decoder.o mq_encoder.o blocks.o codestream.o compressed.o kernels.o messaging.o params.o colour.o analysis.o synthesis.o multi_transform.o roi.o kdu_arch.o kdu_threads.o avx_coder_local.o avx_colour_local.o
	ar -rv $(LIB_DIR)/$(STATIC_LIB_NAME) *.o
	ranlib $(LIB_DIR)/$(STATIC_LIB_NAME)

$(SHARED_LIB_NAME) :: block_coding_common.o block_decoder.o block_encoder.o decoder.o encoder.o mq_decoder.o mq_encoder.o blocks.o codestream.o compressed.o kernels.o messaging.o params.o colour.o analysis.o synthesis.o multi_transform.o roi.o kdu_arch.o kdu_threads.o avx_coder_local.o avx_colour_local.o $(LIB_RESOURCES)
	$(CC) $(CFLAGS) -shared -o $(LIB_DIR)/$(SHARED_LIB_NAME) *.o

kdu_arch.o :: ../common/kdu_arch.cpp
	$(CC) $(CFLAGS) -c ../common/kdu_arch.cpp \
	      -o kdu_arch.o

kdu_threads.o :: ../threads/kdu_threads.cpp
	$(CC) $(CFLAGS) -c ../threads/kdu_threads.cpp \
	      -o kdu_threads.o

mq_encoder.o :: ../coding/mq_encoder.cpp
	$(CC) $(CFLAGS) -c ../coding/mq_encoder.cpp \
	      -o mq_encoder.o
mq_decoder.o :: ../coding/mq_decoder.cpp
	$(CC) $(CFLAGS) -c ../coding/mq_decoder.cpp \
	      -o mq_decoder.o

block_coding_common.o :: ../coding/block_coding_common.cpp
	$(CC) $(CFLAGS) -c ../coding/block_coding_common.cpp \
	      -o block_coding_common.o
block_encoder.o :: ../coding/block_encoder.cpp
	$(CC) $(CFLAGS) -c ../coding/block_encoder.cpp \
	      -o block_encoder.o
block_decoder.o :: ../coding/block_decoder.cpp
	$(CC) $(CFLAGS) -c ../coding/block_decoder.cpp \
	      -o block_decoder.o

encoder.o :: ../coding/encoder.cpp
	$(CC) $(CFLAGS) -c ../coding/encoder.cpp \
	      -o encoder.o
decoder.o :: ../coding/decoder.cpp
	$(CC) $(CFLAGS) -c ../coding/decoder.cpp \
	      -o decoder.o
avx_coder_local.o :: ../coding/avx_coder_local.cpp
	$(CC) $(CFLAGS) $(AVXFLAGS) -c ../coding/avx_coder_local.cpp \
	      -o avx_coder_local.o

codestream.o :: ../compressed/codestream.cpp
	$(CC) $(CFLAGS) -c ../compressed/codestream.cpp \
	      -o codestream.o
compressed.o :: ../compressed/compressed.cpp
	$(CC) $(CFLAGS) -c ../compressed/compressed.cpp \
	      -o compressed.o
blocks.o :: ../compressed/blocks.cpp
	$(CC) $(CFLAGS) -c ../compressed/blocks.cpp \
	      -o blocks.o

kernels.o :: ../kernels/kernels.cpp
	$(CC) $(CFLAGS) -c ../kernels/kernels.cpp \
	      -o kernels.o

messaging.o :: ../messaging/messaging.cpp
	$(CC) $(CFLAGS) -c ../messaging/messaging.cpp \
	      -o messaging.o

params.o :: ../parameters/params.cpp
	$(CC) $(CFLAGS) -c ../parameters/params.cpp \


colour.o :: ../transform/colour.cpp
	$(CC) $(CFLAGS) -c ../transform/colour.cpp \
	      -o colour.o
avx_colour_local.o :: ../transform/avx_colour_local.cpp
	$(CC) $(CFLAGS) $(AVXFLAGS) -c ../transform/avx_colour_local.cpp \
	      -o avx_colour_local.o
analysis.o :: ../transform/analysis.cpp
	$(CC) $(CFLAGS) -c ../transform/analysis.cpp \
	      -o analysis.o
synthesis.o :: ../transform/synthesis.cpp
	$(CC) $(CFLAGS) -c ../transform/synthesis.cpp \
	      -o synthesis.o
multi_transform.o :: ../transform/multi_transform.cpp
	$(CC) $(CFLAGS) -c ../transform/multi_transform.cpp \
	      -o multi_transform.o

roi.o :: ../roi/roi.cpp
	$(CC) $(CFLAGS) -c ../roi/roi.cpp \
	      -o roi.o
