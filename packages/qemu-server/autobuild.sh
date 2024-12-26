#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

apt update
apt install  libglib2.0-dev libjson-c-dev libtest-mockmodule-perl pve-doc-generator pve-edk2-firmware -y
sed -i "s/-b /& -d /" $SCRIPT_DIR/$PKGNAME/Makefile

cd $SCRIPT_DIR/$PKGNAME
exec_build_make
