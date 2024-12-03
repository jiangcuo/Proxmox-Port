#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

cd $SCRIPT_DIR/$PKGNAME
apt update
apt install librust-itertools-dev=0.10.3-1 -y
yes |mk-build-deps --install --remove
exec_build_make
cd $SCRIPT_DIR/$PKGNAME/build
cp *.deb *.buildinfo *.changes *.dsc *.tar* $SCRIPT_DIR/$PKGNAME/ ||true
