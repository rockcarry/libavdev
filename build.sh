#!/bin/bash

# environment needs:
# TOOLCHAIN_NAME       - indicate toolchain name
# BUILD_PACKAGE_OUTDIR - indicate package binary output dir, if empty $PWD/out will be used

set -e

TOPDIR=$PWD

if [ "$BUILD_PACKAGE_OUTDIR"x = ""x ]; then
    BUILD_PACKAGE_OUTDIR=$PWD/out
fi

build_package()
{
    if [ "$TOOLCHAIN_NAME"x = ""x ]; then
        echo -e "\n\033[31menv TOOLCHAIN_NAME not set, please check your toolchain is correctly installed !\033[0m\n"
        exit -1
    fi
    export PKG_CONFIG_PATH=$BUILD_PACKAGE_OUTDIR/$TOOLCHAIN_NAME/lib/pkgconfig

    if [ ! -e $TOPDIR/Makefile ]; then
        ./autogen.sh
        ./configure \
        --host=$BUILD_HOST \
        --prefix=$BUILD_PACKAGE_OUTDIR/$TOOLCHAIN_NAME \
        --disable-shared \
        --enable-static \
        --enable-tests
    fi

    make -j8
    make install
}

case "$1" in
"")
    build_package
    ;;
clean)
    make clean || true
    ;;
distclean)
    ./clean.sh
    ;;
esac

