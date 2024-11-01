#!/bin/bash
PKGNAME="proxmox-archive-keyring"

errlog(){
        echo $1
        exit 1

}

exec_build(){
        apt update
	cp ../pveport.gpg debian/
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
	cd $SH_DIR/$PKGNAME
        make deb
}

echo "This is $PKGNAME build scripts"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

cd $SH_DIR/$PKGNAME
exec_build
