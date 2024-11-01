#!/bin/bash
PKGNAME="swtpm"

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
        dpkg-buildpackage -b -us -uc
}

copy_dir(){
                rsync -ra $SH_DIR/$PKGNAME /build
                cd /build/$PKGNAME
}

echo "This is $PKGNAME build scripts"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

copy_dir
exec_build
cp  /build/*.changes /build/*.buildinfo /build/*.deb $SH_DIR/$PKGNAME
