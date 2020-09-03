#!/bin/sh
boostversion=1_73_0
cd /opt
wget https://dl.bintray.com/boostorg/release/$(echo ${boostversion} | sed "s/_/./g")/source/boost_${boostversion}.tar.bz2 && tar --bzip2 -xf ./boost_${boostversion}.tar.bz2
rm  boost_${boostversion}.tar.bz2
cd  boost_${boostversion} && ./bootstrap.sh --prefix=/usr/local --with-python=python3 --without-icu
./b2 cxxflags="-std=c++17" --disable-icu boost.locale.icu=off boost.locale.iconv=on install



