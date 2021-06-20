#!/bin/sh

echo $0 $1 $2
HOST='172.25.2.24'
USER='ftpuser'
PASSWD='ftpuser'
FILE=$1
DIR=$2



ftp -p -n $HOST <<END_SCRIPT
quote USER $USER
quote PASS $PASSWD
binary
mkdir $DIR
cd $DIR
put $FILE
quit
END_SCRIPT
exit 0
