#!/bin/bash
SH_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SH_DIR)

# submodule.list
#  zfsurl="https://gitea.lierfang.com/proxmox-mirrors/mirror_zfs"
#  ubuntukernelurl="https://gitea.lierfang.com/Proxmox-Port/linux-openeuler"

errlog(){
        echo $1
        exit 1

}

update_submodule(){
	cd $SH_DIR/$PKGNAME
	echo "init submodule"
	if [ -f "$SH_DIR/submodule.list" ];then
		source $SH_DIR/submodule.list
		git submodule set-url src/submodules/ubuntu-kernel "$zfsurl"
		git submodule set-url src/submodules/zfsonlinux  "$ubuntukernelurl"
	fi
	git submodule update --init --recursive --depth=1
}

echo "This is $PKGNAME build scripts"

update_submodule

. $SH_DIR/../common.sh

cd $SH_DIR/$PKGNAME
exec_build_dpkg
