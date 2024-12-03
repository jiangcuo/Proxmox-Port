#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

cd $SCRIPT_DIR/$PKGNAME/ceph

arch=`arch`
if [ "$arch" == "loongarch64" ];then
	for i in `cat $SCRIPT_DIR/series.loongarch64.ceph`;
		do patch -p1 < ../../$i
	done
fi
apt update
yes |mk-build-deps --install --remove
cd $SCRIPT_DIR/$PKGNAME
exec_build_make
