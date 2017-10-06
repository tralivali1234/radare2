# Ugly yet that's the path inside dockcross
ifeq (${PATH},"/usr/arm-linux-androideabi/bin/${ARCH}-linux-androideabi-gcc")
CC=${ARCH}-linux-androideabi-gcc
RANLIB={ARCH}-linux-androideabi-ranlib
USERCC=${ARCH}-linux-androideabi-gcc -fPIC -fPIE
else
CC=ndk-gcc -fPIC -fPIE
#RANLIB=ndk-ranlib
USERCC=ndk-gcc -fPIC -fPIE
endif

ARCH=arm

ifeq (${NDK_ARCH},x86)
# mips
ARCH2=i686
RANLIB=${ARCH2}-linux-android-ranlib
AR=${ARCH2}-linux-android-ar
CC_AR=${AR} -r ${LIBAR}
endif

ifeq (${NDK_ARCH},mips)
# mips
ARCH2=mipsel
RANLIB=${ARCH2}-linux-android-ranlib
AR=${ARCH2}-linux-android-ar
CC_AR=${AR} -r ${LIBAR}
endif

ifeq (${NDK_ARCH},mips64)
# mips
ARCH2=mips64el
RANLIB=${ARCH2}-linux-android-ranlib
AR=${ARCH2}-linux-android-ar
CC_AR=${AR} -r ${LIBAR}
endif

ifeq (${NDK_ARCH},arm)
# arm32
ARCH=arm
RANLIB=${ARCH}-linux-androideabi-ranlib
AR=${ARCH}-linux-androideabi-ar
CC_AR=${AR} -r ${LIBAR}
endif

ifeq (${NDK_ARCH},aarch64)
# aarch64
ARCH=aarch64
RANLIB=${ARCH}-linux-android-ranlib
AR=${ARCH}-linux-android-ar
CC_AR=${AR} -r ${LIBAR}
endif
ONELIB=0
OSTYPE=android
LINK=
#CC_AR=ndk-ar -r ${LIBAR}
PICFLAGS=-fPIC -fpic
CFLAGS+=${PICFLAGS}
CC_LIB=${CC} -shared -o
CFLAGS_INCLUDE=-I
LDFLAGS_LINK=-l
LDFLAGS_LINKPATH=-L
CFLAGS_OPT0=-O0
CFLAGS_OPT1=-O1
CFLAGS_OPT2=-O2
CFLAGS_OPT3=-O3
CFLAGS_DEBUG=-g
