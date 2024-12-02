#!/bin/bash
PKGNAME="pve-qemu"

errlog(){
        echo $1
        exit 1

}

update_submodule(){
	cd $SH_DIR/$PKGNAME
	echo "init submodule"
	git submodule update --init --depth=1 || errlog "submodule update failed"
	cd $SH_DIR/$PKGNAME/qemu
	git submodule update --init --depth=1
	# replace Zeex/subhook
	echo "set submodule url for subhook"
	cd  $SH_DIR/$PKGNAME/qemu/roms/edk2/
	git submodule set-url UnitTestFrameworkPkg/Library/SubhookLib/subhook https://github.com/tianocore/edk2-subhook
	git submodule update --init --recursive --depth=1
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

update_submodule
cd $SH_DIR/$PKGNAME
exec_build
