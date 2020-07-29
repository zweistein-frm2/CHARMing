#!/bin/sh
sudo sysctl -w kernel.sched_rt_runtime_us=-1
sudo -E ./charm $1