#!/bin/sh

cd /package

for i in *.*

do

if [ -f "$i" ];
    then
        printf "Filename: %s\n" "${i##*/}" # longest prefix removal
        /CHARMing/distribution/ftpout.sh $i $LINUX_FLAVOUR

    fi

done
