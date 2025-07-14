#!/bin/bash

# by default compile kernel for S8 (g950x_defconfig)
# if you want to build for S8+ korean version, use "g955x_kor_defconfig" etc..

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
