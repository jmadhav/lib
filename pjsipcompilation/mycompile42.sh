#!/bin/sh

export DEV=/Developer/Platforms/iPhoneOS.platform/Developer
export SDK=${DEV}/SDKs/iPhoneOS3.1.sdk
find . -name "*.depend" -print -exec rm -f {} \;
find . -name "*arm*.a" -print -exec rm -f {}  \;
find . -name "*.o" -print -exec rm -f {}  \;

pushd ${DEV}/usr/bin


rm  /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-gcc
rm  /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-g++
rm  /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-ranlib

ln -s gcc-4.2 arm-apple-darwin9-gcc
ln -s gcc-4.2 arm-apple-darwin9-g++
ln -s ranlib arm-apple-darwin9-ranlib
popd

export PATH=${DEV}/usr/bin:${PATH}

export CFLAGS="-O2 -arch armv6   -isysroot ${SDK}"

export LDFLAGS="-O2 -arch armv6  -isysroot ${SDK}"

export CPP="${DEV}/usr/bin/cpp"
#--disable-speex-aec --disable-speex-codec
./aconfigure --host=arm-apple-darwin9 --disable-ssl \
 --disable-l16-codec --disable-g722-codec \
--disable-ilbc-codec CFLAGS="-arch  armv6   -DPA_USE_COREAUDIO=1  -pipe -O0 -isysroot \
 /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.1.sdk \
-I/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.1.sdk/usr/include/gcc/darwin/4.0" \
LDFLAGS="-isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.sdk \
-L/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/IphoneOS3.1.sdk/usr/lib" \
#CPP=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-cpp \
AR=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-ar \
RANLIB=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-ranlib \
CC=/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/arm-apple-darwin9-gcc


make dep
make

