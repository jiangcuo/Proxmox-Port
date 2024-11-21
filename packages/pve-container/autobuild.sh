#!/bin/bash
SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE[0]}); pwd)
PKGNAME=$(basename $SCRIPT_DIR)

echo "This is $PKGNAME build scripts"

. ../common.sh

cd $SCRIPT_DIR/$PKGNAME

if [ "$(ls -di /)" != "$(ls -di /proc/1/root)" ]; then
	export DEB_BUILD_OPTIONS=nocheck
fi

if grep -qa container /proc/1/cgroup; then
	export DEB_BUILD_OPTIONS=nocheck
fi

exec_build_make
