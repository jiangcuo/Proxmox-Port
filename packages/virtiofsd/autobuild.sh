#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

. ../common.sh
apt update
apt install librust-env-logger-dev=0.10.0-1~bpo12+pve1 -y
export DEB_BUILD_OPTIONS="nocheck"

copy_dir
exec_build_dpkg
cp  /build/*.changes /build/*.buildinfo /build/*.deb /build/*.tar.* /build/*.dsc $SH_DIR/$PKGNAME ||true
