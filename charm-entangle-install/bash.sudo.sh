#!/bin/bash
[ "$UID" -eq 0 ] || exec sudo "$@"
