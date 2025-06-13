#!/bin/bash

export LD_PRELOAD=../libredbox.so
var=$[$(date +%s%N)/1000000]
export RB_STARTUP_TIME=$var
./dde-file-manager
