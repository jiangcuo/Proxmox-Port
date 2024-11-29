#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

for i in common/pkg  pve-rs;
do
	cd $SCRIPT_DIR/$PKGNAME/$i
	exec_build_make
	cp *.buildinfo *.changes *.dsc *.tar* *.deb $SCRIPT_DIR/$PKGNAME/ ||true
done
