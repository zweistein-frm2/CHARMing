#!/bin/sh
[ ! -d "/etc/charm/" ] && sudo mkdir /etc/charm && sudo chmod 666 /etc/charm
sudo sysctl -w kernel.sched_rt_runtime_us=-1
[ -z "$SSH_CLIENT" ] && docker run -it  --rm --net=host -e DISPLAY=$DISPLAY  -v /tmp/X11-unix:/tmp/.X11-unix -v /etc/charm:/etc/charm -v $HOME:/root/ --sysctl net.core.rmem_max=26214400 --ulimit rtprio=98  --cap-add=sys_nice charming
[ "$SSH_CLIENT" ] &&  docker run -it --rm --net=host --volume="$HOME/.Xauthority:/root/.Xauthority:rw" --env="DISPLAY"  -v /etc/charm:/etc/charm -v $HOME:/root/ --sysctl net.core.rmem_max=26214400 --ulimit rtprio=98  --cap-add=sys_nice charming
 