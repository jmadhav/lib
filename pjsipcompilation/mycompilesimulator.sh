#!/bin/sh
export DEV=/Developer/Platforms/iPhoneSimulator.platform/Developer/
export SDK=${DEV}/SDKs/iPhoneSimulator3.0.sdk
find . -name "*.depend" -print -exec rm -f {} \;
find . -name "*i386*.a" -print -exec rm -f {}  \;
find . -name "*.o" -print -exec rm -f {}  \;
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

#export CFLAGS="-O2 -arch i386 -isysroot ${SDK}"

export  CFLAGS="-arch i386 -pipe -O0 -isysroot  /Developer/Platforms/iPhoneSimulator.platform/Developer/SDKs/iPhoneSimulator3.0.sdk -I $SDK/usr/include/gcc/darwin/4.0"

export LDFLAGS="-O2 -arch i386 -isysroot ${SDK}"
#export LDFLAGS="-isysroot $SDK -L $SDK/usr/lib" 
export CPP="${DEV}/usr/bin/cpp"

./aconfigure --host=i386-apple-darwin9 --disable-ssl \
--disable-speex-aec   --disable-l16-codec --disable-g722-codec \
--disable-ilbc-codec \
#CPP=/Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/simulator-apple-darwin9-cpp \
AR=/Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/simulator-apple-darwin9-ar \
RANLIB=/Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/simulator-apple-darwin9-ranlib \
CC=/Developer/Platforms/iPhoneSimulator.platform/Developer/usr/bin/simulator-apple-darwin9-gcc
  

make dep
make
