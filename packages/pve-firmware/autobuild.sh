#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

cp $SCRIPT_DIR/fwlist* $SCRIPT_DIR/$PKGNAME

apt update
yes |mk-build-deps --install --remove
cd $SCRIPT_DIR/$PKGNAME
exec_build_make
