SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./build}
mkdir $BUILD_DIR
rm -fr $BUILD_DIR/*
cd $BUILD_DIR
cmake ..
make