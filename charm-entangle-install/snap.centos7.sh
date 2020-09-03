#!/bin/sh
sudo yum install epel-release
sudo yum install yum-plugin-copr
sudo yum copr enable ngompa/snapcore-el7 -y
sudo yum -y install snapd
sudo systemctl enable --now snapd.socket
sudo ln -s /var/lib/snapd/snap /snap

