#
# Build the OpenVPN down-root plugin module.
#

# This directory is where we will look for openvpn-plugin.h
INCLUDE=-I../..

CC=gcc-4.0
MACOSX_DEPLOYMENT_TARGET=10.4
SDK=/Developer/SDKs/MacOSX10.4u.sdk

CC_FLAGS=-Wall -isysroot $(SDK) -Os -mmacosx-version-min=$(MACOSX_DEPLOYMENT_TARGET) -arch i386 -arch ppc

down-root.so : down-root.o
	$(CC) ${CC_FLAGS} -fPIC -bundle -Wl,-dylib_install_name,openvpn-down-root.so -o openvpn-down-root.so down-root.o -lc

down-root.o : down-root.c
	$(CC) ${CC_FLAGS} -fPIC -c ${INCLUDE} down-root.c

clean :
	rm -f *.o *.so
