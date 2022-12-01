#!/bin/bash

# if you have a quad-core CPU with 2-threads per-cpu, then use JOBS=8

# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
JOBS=2

cd $SOURCE_PATH
make -j$JOBS clean
make -j$JOBS distclean
