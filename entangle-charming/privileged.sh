#!/bin/sh
sudo sysctl -w kernel.sched_rt_runtime_us=-1
sudo python3 bin/entangle-server $1

