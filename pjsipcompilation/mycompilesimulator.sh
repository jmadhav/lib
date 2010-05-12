#!/bin/sh
export DEV=/Developer/Platforms/iPhoneSimulator.platform/Developer
export SDK=${DEV}/SDKs/iPhoneSimulator3.1.sdk
find . -name "*i386*.depend" -print -exec rm -f {} \;
find . -name "*i386*.a" -print -exec rm -f {}  \;
find . -name "*i386*.o" -print -exec rm -f {}  \;
pushd ${DEV}/usr/bin

rm  /Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/i386-apple-darwin9-gcc
rm  /Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/i386-apple-darwin9-g++
rm  /Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/i386-apple-darwin9-ranlib
echo pwd

ln -s gcc-4.2 i386-apple-darwin9-gcc
ln -s gcc-4.2 i386-apple-darwin9-g++
ln -s ranlib  i386-apple-darwin9-ranlib
popd


export PATH=${DEV}/usr/bin:${PATH}

export CFLAGS="-O2 -arch i386   -isysroot ${SDK}"

export LDFLAGS="-O2 -arch i386  -isysroot ${SDK}"

export CPP="${DEV}/usr/bin/cpp"
#--disable-speex-aec --disable-speex-codec
./aconfigure --host=i386-apple-darwin9 --disable-ssl \
 --disable-l16-codec --disable-g722-codec \
--disable-ilbc-codec CFLAGS="-mmacosx-version-min=10.5 -arch  i386   -DPA_USE_COREAUDIO=1  -pipe -O0 -isysroot \
 /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator3.1.sdk \
 -I/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator3.1.sdk/usr/include/gcc/darwin/4.0" \
LDFLAGS="-mmacosx-version-min=10.5 -isysroot /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator3.1.sdk \
-L/Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator3.1.sdk/usr/lib" \
#CPP=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/i386-apple-darwin9-cpp \
AR=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/i386-apple-darwin9-ar \
RANLIB=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/i386-apple-darwin9-ranlib \
CC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/i386-apple-darwin9-gcc


make dep
make















