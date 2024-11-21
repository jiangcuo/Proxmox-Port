#!/bin/bash
PKGNAME="pve-qemu"

errlog(){
        echo $1
        exit 1

}

exec_build(){
        apt update
        apt install libpve-access-control librados2  librados-dev -y
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
	cd $SH_DIR/$PKGNAME/qemu
	meson subprojects download
	cd $SH_DIR/$PKGNAME
	make dsc || errlog "make dsc error"
        make deb || errlog "make deb error"
}

echo "This is $PKGNAME build scripts"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

cd $SH_DIR/$PKGNAME
exec_build
