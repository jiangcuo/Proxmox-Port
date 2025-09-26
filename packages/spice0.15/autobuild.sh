#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

. ../common.sh

apt update && apt install libspice-protocol-dev=0.14.3-2 -y

copy_dir
exec_build_dpkg
cp  /build/*.changes /build/*.buildinfo /build/*.deb  $SH_DIR/$PKGNAME
