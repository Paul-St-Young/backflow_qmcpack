#!/bin/bash

# refresh executable on Titan
# toolchain file should have everything
#source ../env-for-qmcpack.sh

if [ ! -d build ]; then
  mkdir build
  cd build
  cmake -DCMAKE_TOOLCHAIN_FILE=../config/TitanGNU.cmake ..
  sed -i 's/\/sw\/xk6\/subversion\/1.8.3\/sles11.1_gnu4.3.4\/bin\/svn//' CMakeCache.txt
  cmake -DCMAKE_TOOLCHAIN_FILE=../config/TitanGNU.cmake .. # run a second time to pass in C flags
else
  cd build
fi

make qmcapp -j 16 

cd ..
