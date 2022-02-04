SOURCE_DIR=`pwd`
BUILD_DIR=${BUILD_DIR:-./build}
cd $BUILD_DIR
rm -fr *
cmake ..
make