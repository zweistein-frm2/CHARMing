#!/usr/bin/env bash
git submodule update --init --recursive

chmod +x buildboost.sh
./buildboost.sh $1

echo 'OK' >> REPOSITORY_INIT_OK
