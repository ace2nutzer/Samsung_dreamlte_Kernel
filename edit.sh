#!/bin/bash

# by default compile kernel for S8 (g950x_defconfig)
# if you want to build for S8+ korean version, use "g955x_kor_defconfig" etc..
# if you have a quad-core CPU with 2-threads per-cpu, then use JOBS=8

# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
DEFCONFIG=g950x_defconfig
JOBS=2

cd $SOURCE_PATH

make -j$JOBS $DEFCONFIG

make -j$JOBS menuconfig

cp .config arch/arm64/configs/$DEFCONFIG
