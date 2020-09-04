#!/usr/bin/env bash

chmod +x buildboost.sh
chmod +x boost/bootstrap.sh
chmod +x boost/tools/build/bootstrap.sh
chmod +x boost/tools/build/src/engine/build.sh

cd boost

echo $PWD/out

./b2 --clean
rm -rf bin.v2/config.log
rm -rf bin.v2/project-cache.jam

./bootstrap.sh  --with-python=python3 --without-icu  --prefix=$PWD/out
./b2 link=static,shared cxxflags=-std=c++11  cflags=-fPIC address-model=64 --disable-icu boost.locale.icu=off boost.locale.iconv=on install $1 -sNO_LZMA=1 -sNO_ZLIB=1 -sNO_BZIP2=1 --prefix=$PWD/out
cd ..