#!/bin/bash
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
}

echo "This is pve-xtermjs build scripts"
PKGNAME="pve-xtermjs"


SH_PATH=$(realpath "$0")
SH_DIR=$(dirname $SH_PATH)

for i in xterm.js  termproxy;
do
	cd $SH_DIR/pve-xtermjs/$i
	exec_build
done

