#!/bin/bash

# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
N=$(nproc)

cd $SOURCE_PATH
make -j$N clean
make -j$N distclean
