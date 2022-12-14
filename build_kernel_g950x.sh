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

set -e
USER_AGENT="WireGuard-AndroidROMBuild/0.3 ($(uname -a))"

exec 9>.wireguard-fetch-lock
flock -n 9 || exit 0

[[ $(( $(date +%s) - $(stat -c %Y "net/wireguard/.check" 2>/dev/null || echo 0) )) -gt 86400 ]] || exit 0

while read -r distro package version _; do
	if [[ $distro == upstream && $package == linuxcompat ]]; then
		VERSION="$version"
		break
	fi
done < <(curl -A "$USER_AGENT" -LSs --connect-timeout 30 https://build.wireguard.com/distros.txt)

[[ -n $VERSION ]]

if [[ -f net/wireguard/version.h && $(< net/wireguard/version.h) == *$VERSION* ]]; then
	touch net/wireguard/.check
	exit 0
fi

rm -rf net/wireguard
mkdir -p net/wireguard
curl -A "$USER_AGENT" -LsS --connect-timeout 30 "https://git.zx2c4.com/wireguard-linux-compat/snapshot/wireguard-linux-compat-$VERSION.tar.xz" | tar -C "net/wireguard" -xJf - --strip-components=2 "wireguard-linux-compat-$VERSION/src"
sed -i 's/tristate/bool/;s/default m/default y/;' net/wireguard/Kconfig
touch net/wireguard/.check


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
