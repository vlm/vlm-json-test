
INSTALL = install
CFLAGS ?= -O2 -g -fPIC
CC ?= gcc

# Default on Mac OS X
libvlm-json-test.dylib: vlm-json-test.o
	libtool -dynamic -lc -o libvlm-json-test.dylib vlm-json-test.o

# Static version, use this on your platform if !Mac.
libvlm-json-test.a: vlm-json-test.o
	ar rc libvlm-json-test.a vlm-json-test.o

vlm-json-test.o: vlm-json-test.c vlm-json-test.h
	$(CC) $(CFLAGS) -c -o vlm-json-test.o vlm-json-test.c

install: libvlm-json-test.dylib
	$(INSTALL) libvlm-json-test.dylib /usr/local/lib
	$(INSTALL) vlm-json-test.h /usr/local/include

