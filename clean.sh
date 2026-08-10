#!/bin/bash

set -e
trap 'echo; echo FAILED; echo' ERR

# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
N=$(nproc)

cd $SOURCE_PATH
make -j$N clean
make -j$N mrproper

echo
echo DONE
echo
