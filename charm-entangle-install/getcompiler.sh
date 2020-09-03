#!/bin/sh
add-apt-repository -y ppa:ubuntu-toolchain-r/test
apt-get update
apt-get install -y gcc g++
gccinstalled=$(echo $(gcc --version | sed -e 's/([^()]*)//g' | sed 's/[^0-9]*//g' ) | sed 's/\s.*$//')
gccinstalled=$(gcc --version | head -n1 | cut -d" " -f4)

gccmajor=$(echo $gccinstalled | sed 's/.//2g')
echo $gccmajor


if [ $gccmajor -lt 9 ]

   then
		echo $gccinstalled
		priority=$(echo $gccinstalled | sed 's/[^0-9]*//g')
		echo $priority
		update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-$gccmajor $priority --slave /usr/bin/g++ g++ /usr/bin/g++-$gccmajor
        apt-get install -y gcc-9 g++-9
        update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 900 --slave /usr/bin/g++ g++ /usr/bin/g++-9
fi
