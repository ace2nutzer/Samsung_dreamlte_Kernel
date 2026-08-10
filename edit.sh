#!/bin/bash

set -e
trap 'echo; echo FAILED; echo' ERR

# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
DEFCONFIG=g950x_defconfig
N=$(nproc)

cd $SOURCE_PATH

make -j$N $DEFCONFIG
make -j$N menuconfig
cp .config arch/arm64/configs/$DEFCONFIG
make -j$N $DEFCONFIG
cp .config arch/arm64/configs/$DEFCONFIG

echo
echo DONE
echo
