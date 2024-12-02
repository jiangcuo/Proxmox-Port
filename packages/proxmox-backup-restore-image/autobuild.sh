#!/bin/bash
SH_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SH_DIR)

# submodule.list
#  zfsurl="https://gitea.lierfang.com/proxmox-mirrors/mirror_zfs"
#  ubuntukernelurl="https://gitea.lierfang.com/Proxmox-Port/linux-openeuler"


update_submodule(){
	cd $SH_DIR/$PKGNAME
	echo "init submodule"
	if [ -f "$SH_DIR/submodule.list" ];then
		source $SH_DIR/submodule.list
		git submodule set-url src/submodules/ubuntu-kernel  "$ubuntukernelurl" || errlog "failed to update kernel"
		git submodule set-url submodules/zfsonlinux  "$zfsurl" || errlog "failed to update zfs"
	fi
	git submodule update --init --recursive --depth=1
}

echo "This is $PKGNAME build scripts"

. $SH_DIR/../common.sh

update_submodule

cd $SH_DIR/$PKGNAME
exec_build_make
