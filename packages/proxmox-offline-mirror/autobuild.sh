#!/bin/bash
PKGNAME="proxmox-offline-mirror"

errlog(){
        echo $1
        exit 1

}

exec_build(){
        apt update
        apt install librust-proxmox-router-dev=2.2.4-1 librust-proxmox-router-2+cli-dev=2.2.4-1 -y
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
        make deb
}

echo "This is $PKGNAME build scripts"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

cd $SH_DIR/$PKGNAME
exec_build
