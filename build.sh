#!/bin/bash

set -e
trap 'echo; echo FAILED; echo' ERR

# By default compile kernel for S8 European model (g950x).
# If you need to build for S8+, just uncomment model "g955x" and comment model "g950x".

# SETUP

### S8 ###
MODEL=g950x

### S8 Korea version ###
#MODEL=g950x_kor

### S8+ ###
#MODEL=g955x

### S8+ Korea ###
#MODEL=g955x_kor

### Note 8 ###
#MODEL=n950x

### Note 8 Korea ###
#MODEL=n950x_kor

SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
N=$(nproc)
OUTPUT=$HOME/a2n_kernel_$MODEL_9.x
AIK=$HOME/AIK-Linux
DTB=arch/arm64/boot/dts/exynos/*dtb*

	cd $SOURCE_PATH

	if [ -f $DTB ] ; then
		rm $DTB
	fi

	ARCH=arm64 scripts/kconfig/merge_config.sh arch/arm64/configs/g950x_defconfig arch/arm64/configs/$MODEL_defconfig
	make -j$N $@

	# copy modules
	cp drivers/usb/gadget/function/usb_f_mtp_samsung.ko $OUTPUT/system/lib/modules
	cp drivers/usb/gadget/function/usb_f_ptp_samsung.ko $OUTPUT/system/lib/modules
	cp net/wireguard/wireguard.ko $OUTPUT/system/lib/modules

	cp arch/arm64/boot/Image $AIK/split_img/boot.img-kernel

	./tools/dtbtool -o $AIK/split_img/boot.img-dt arch/arm64/boot/dts/exynos/

	cd $AIK/

	./repackimg.sh --nosudo

	cp image-new.img $OUTPUT/boot.img

	cd $OUTPUT/

	if [ -f *.zip ] ; then
		rm *.zip
	fi

	if [ -f *.md5 ] ; then
		rm *.md5
	fi

	zip -r a2n_kernel_$MODEL_9.x_user_build.zip META-INF system dex2oat_patch boot.img

echo
echo DONE
echo
