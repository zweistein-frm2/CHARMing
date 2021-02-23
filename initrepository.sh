#!/usr/bin/env bash
git submodule update --init --recursive
chmod +x opencv_setup.sh
./opencv_setup.sh
chmod +x buildboost.sh
./buildboost.sh $1

echo 'OK' >> REPOSITORY_INIT_OK
