#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

. ../common.sh

ARCH=`arch`

apt update

if [ "$ARCH" == "aarch64"  ];then
apt install debhelper=13.19~bpo12+1 libdebhelper-perl=13.19~bpo12+1 -y
fi

apt install debhelper dh-exec dh-runit libaudit-dev libedit-dev libfido2-dev libgtk-3-dev libkrb5-dev libpam0g-dev libselinux1-dev libssl-dev libwrap0-dev zlib1g-dev  -y

copy_dir
exec_build_dpkg
cp  /build/*.changes /build/*.buildinfo /build/*.deb /build/*.tar.* /build/*.dsc $SH_DIR/$PKGNAME ||true
