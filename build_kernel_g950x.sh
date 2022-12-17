#!/bin/bash

# by default compile kernel for S8 (g950x_defconfig)
# if you want to build for S8+ korean version, use "g955x_kor_defconfig" etc..
# if you have a quad-core CPU with 2-threads per-cpu, then use JOBS=8

# Created By @ace2nutzer xda
# Modified By @v3rb0se xda

echo "==========================="
echo " Fetching Latest Wireguard "
echo "==========================="
# Fetching Missing Wireguard 


bash fetch-wireguard.sh


echo "========================"
echo " Setting up Environment "
echo "========================"


# SETUP
SOURCE_PATH=$HOME/Samsung_dreamlte_Kernel
DEFCONFIG=g950x_defconfig
JOBS=2
OUTPUT=$HOME/a2n_kernel_g950x_9.x
AIK=$HOME/AIK-Linux

	cd $SOURCE_PATH

	rm arch/arm64/boot/dts/exynos/*dtb*

echo "==================="
echo " Compilation Start "
echo "==================="


	make -j$JOBS $DEFCONFIG

	make -j$JOBS $@
	
	
echo "================="
echo " Compilation End "
echo "================="


echo "==============="
echo " Zipping Stuff "
echo "==============="

	# copy modules
	cp fs/cifs/cifs.ko $OUTPUT/system/lib/modules
	cp crypto/md4.ko $OUTPUT/system/lib/modules
	cp fs/fscache/fscache.ko $OUTPUT/system/lib/modules
	cp net/dns_resolver/dns_resolver.ko $OUTPUT/system/lib/modules
	cp drivers/usb/gadget/function/usb_f_mtp_samsung.ko $OUTPUT/system/lib/modules
	cp drivers/usb/gadget/function/usb_f_ptp.ko $OUTPUT/system/lib/modules
	cp fs/ntfs/ntfs.ko $OUTPUT/system/lib/modules

	cp fs/nfs_common/grace.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfs.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv2.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv3.ko $OUTPUT/system/lib/modules
	cp fs/nfs/nfsv4.ko $OUTPUT/system/lib/modules
	cp fs/lockd/lockd.ko $OUTPUT/system/lib/modules
	cp net/sunrpc/sunrpc.ko $OUTPUT/system/lib/modules
	cp net/sunrpc/auth_gss/auth_rpcgss.ko $OUTPUT/system/lib/modules
	cp lib/oid_registry.ko $OUTPUT/system/lib/modules

	cp net/ipv4/udp_tunnel.ko $OUTPUT/system/lib/modules
	cp net/ipv6/ip6_udp_tunnel.ko $OUTPUT/system/lib/modules
	cp net/wireguard/wireguard.ko $OUTPUT/system/lib/modules
	cp net/l2tp/l2tp_core.ko $OUTPUT/system/lib/modules

	# copy modules to ramdisk
	cp crypto/crc32.ko $AIK/ramdisk/lib/modules
	cp fs/f2fs/f2fs.ko $AIK/ramdisk/lib/modules

	cp arch/arm64/boot/Image $AIK/split_img/boot.img-kernel

	./tools/dtbtool -o $AIK/split_img/boot.img-dt arch/arm64/boot/dts/exynos/

	cd $AIK/

	./repackimg.sh --nosudo

	cp image-new.img $OUTPUT/boot.img

	cd $OUTPUT/

	rm *.zip

	zip -r a2n_kernel_g950x_9.x_user_build.zip META-INF system boot.img

	md5sum *.zip > *.md5
	
	
	echo "Finished"
	echo "you can find generated zip file inside $OUTPUT"
