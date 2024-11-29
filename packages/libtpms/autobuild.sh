#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

. ../common.sh

copy_dir
exec_build_dpkg
cp -r /build/*.changes /build/*.buildinfo /build/*.deb /build/*.tar.* /build/*.dsc $SH_DIR/$PKGNAME ||true
