#!/bin/bash
PKGNAME="proxmox-perl-rs"

errlog(){
        echo $1
        exit 1

}

exec_build(){
	apt update 
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
        make deb
        make dsc
}

echo "This is $PKGNAME build scripts"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

for i in common/pkg  pve-rs;
do
	cd $SH_DIR/$PKGNAME/$i
	exec_build
done

