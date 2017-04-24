#!/bin/bash
BASEDIR=$(dirname "$0")
NDK_DIR=$1
if [ $# -eq 2 ]
then
    COMPILE_SINGLE=$2
fi

if ! [ -d "$NDK_DIR" ];
then
    echo "NDK_DIR is not a directory"
    exit
fi

BUILD_DIR=$BASEDIR"/build"
API_LEVEL=21
TOOLCHAIN_PATH="/tmp/ndk_toolchain_cc"
TOOLCHAIN_SCRIPT=$NDK_DIR"/build/tools/make_standalone_toolchain.py --api $API_LEVEL"
PLATFORMS=("arm" "arm64" "mips" "mips64" "x86" "x86_64")

mkdir -p $BUILD_DIR
for PLATFORM in "${PLATFORMS[@]}"
do
    if ! [ -z ${COMPILE_SINGLE+x} ]
    then
        if ! [ $2 == $PLATFORM ]
        then
            echo "Skipping $PLATFORM"
            continue
        else
            echo "Will compile $PLATFORM"
        fi
    fi
    
    echo " ==== Now building for $PLATFORM === "
    # Create toolchain
    if ! [ -d "$TOOLCHAIN_PATH/$PLATFORM" ]
    then
        echo "Creating toolchain $PLATFORM"
        CREATE_TOOLCHAIN=$TOOLCHAIN_SCRIPT" --install-dir $TOOLCHAIN_PATH/$PLATFORM --arch $PLATFORM"
        $CREATE_TOOLCHAIN
    fi
    
    export ANDROID_CROSS_COMPILE="$TOOLCHAIN_PATH/$PLATFORM/bin"
    export ANDROID_AS=$ANDROID_CROSS_COMPILE"/llvm-as"
    export ANDROID_CC=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -name '*-gcc-[0-9].[0.9]')
    export ANDROID_CXX=$ANDROID_CROSS_COMPILE"/clang++"
    export ANDROID_LD=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -name '*-ld')
    export ANDROID_OBJCOPY=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -name '*android*-objcopy')
    export ANDROID_SIZE=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -name '*android*-size')
    export ANDROID_STRIP=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -name '*android*-strip')
    export ANDROID_AR=$(find $TOOLCHAIN_PATH/$PLATFORM/bin -regex '[A-za-z0-9\_/\-]*android[A-Za-z]*-ar')
    export ANDROID_PLATFORM=$PLATFORM

    if [ -d $ANDROID_CROSS_COMPILE ]\
        && [ -x "$ANDROID_AS" ] && [ -x "$ANDROID_CC" ]\
        && [ -x "$ANDROID_CXX" ] && [ -x "$ANDROID_LD" ]\
        && [ -x "$ANDROID_OBJCOPY" ] && [ -x "$ANDROID_SIZE" ]\
        && [ -x "$ANDROID_STRIP" ] && [ -x "$ANDROID_AR" ]
    then
        echo "Found NDK tools"
    else
       echo "Could not find all following NDK tools:"
       echo $ANDROID_CROSS_COMPILE
       echo $ANDROID_AS
       echo $ANDROID_CC
       echo $ANDROID_CXX
       echo $ANDROID_LD
       echo $ANDROID_OBJCOPY
       echo $ANDROID_SIZE
       echo $ANDROID_STRIP 
       echo $ANDROID_AR
   fi
    
    make -f "$BASEDIR/Makefile_mpy" clean
    make -f "$BASEDIR/Makefile_mpy"

    # Also copy mpy lib into build dir
    mkdir -p "$BUILD_DIR/$PLATFORM"
    cp $BASEDIR/../../libmpy/libmicropython.a $BUILD_DIR/$PLATFORM/libmicropython.a

    #exit
done
echo "Done"
