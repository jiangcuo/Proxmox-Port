#!/bin/bash
PKGNAME=$1
SH_PATH=$(readlink -f `dirname "$0"`)

errlog(){
   echo $1;
   exit 1;
}

if [ ! -n "$PKGNAME" ];then
	errlog "Useage: ./build.sh pve-common"
fi

if [ ! -d "$SH_PATH/packages/$PKGNAME" ];then
	errlog "$PKGNAME is not exsited!"
fi

dockerbuild(){
	rm $SH_PATH/packages/$PKGNAME/$PKGNAME/pvebuild -rf
	if [ -n "$BUILDERNAME" ];then
		docker run -it -e PKGDIR=$SH_PATH/packages/$PKGNAME/$PKGNAME -v $SH_PATH/:$SH_PATH --name $PKGNAME --rm $BUILDERNAME || errlog "builderror"
	else
		docker run -it -e PKGDIR=$SH_PATH/packages/$PKGNAME/$PKGNAME  -v $SH_PATH/:$SH_PATH --name $PKGNAME --rm pvebuilder || errlog "builderror"
	fi
}

upload_pkg(){
	mkdir /tmp/$PKGNAME -p
	find "$SH_PATH/packages/$PKGNAME/$PKGNAME" -name "*.deb" -exec cp {} /tmp/$PKGNAME \;
	for i in `ls /tmp/$PKGNAME/*.deb`;
		do
		md5sum $i > $i.md5
		cat $i.md5
	done
	find "$SH_PATH/packages/$PKGNAME/$PKGNAME" -name "*.deb.md5" -exec cp {} /tmp/$PKGNAME \;
	find "$SH_PATH/packages/$PKGNAME/$PKGNAME" -name "*.buildinfo" -exec cp {} /tmp/$PKGNAME \;
	find "$SH_PATH/packages/$PKGNAME/$PKGNAME" -name "*.changes" -exec cp {} /tmp/$PKGNAME \;
}

update_submodues(){
	if [ -d "$SH_PATH/packages/$PKGNAME/$PKGNAME/.git/" ]; then
		echo "skip submodule"
	else
		cd $SH_PATH/packages/$PKGNAME/
		git submodule update --init  --recursive "$PKGNAME"
	fi
}

update_submodues || errlog  "Failed to update submodule"


ARCH=$(arch)

if [ -f "$SH_PATH/packages/$PKGNAME/series" ];then
	cat $SH_PATH/packages/$PKGNAME/series > $SH_PATH/packages/$PKGNAME/series.all
fi

if [ -f "$SH_PATH/packages/$PKGNAME/series.$ARCH" ];then
	cat $SH_PATH/packages/$PKGNAME/series.$ARCH >> $SH_PATH/packages/$PKGNAME/series.all
fi

if [ -f "$SH_PATH/packages/$PKGNAME/series.all" ];then
	echo "apply patches for series.all"
	cd "$SH_PATH/packages/$PKGNAME/$PKGNAME"
	QUILT_PATCHES=../ \
	QUILT_SERIES=../series.all \
	quilt --quiltrc /dev/null --color=always push -a  || test $$? = 2
fi

cd $SH_PATH

dockerbuild
upload_pkg || errlog "upload pkg failed"

