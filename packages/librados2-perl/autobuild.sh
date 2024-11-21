#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)
. ../common.sh
echo "This is $PKGNAME build scripts"


exec_build(){
        apt update
        apt install libpve-access-control librados2  librados-dev -y
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
        make deb || errlog "build deb error"
	if [ $dscflag == "dsc" ];then
		make dsc || errlog "build dsc error"
	fi
}

cd $SCRIPT_DIR/$PKGNAME
exec_build
