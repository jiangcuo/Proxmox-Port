#!/bin/bash
errlog(){
        echo $1
        exit 1

}

export dscflag="dsc"

if [[  "$DEB_BUILD_OPTIONS" == *"nodsc"*   ]];then
	dscflag="nodsc"
fi

exec_build(){
	echo "install depends"
	apt update
        yes |mk-build-deps --install --remove
	mkdir /tmp/$PKGDIR -p
        if [ -f "Makefile" ];then
		echo "clean "
		make clean || echo ok
		echo "build deb in `pwd` "
		if [ $dscflag == "dsc" ];then
			make dsc || errlog "build  dsc error"
			cp *.dsc *.tar.* /tmp/$PKGDIR
		fi
                make deb || errlog "build  deb error"
		# We need copy deb files first beacuse of deb will be clean when dsc build
		cp -r /tmp/$PKGDIR/* ./
        else
		dpkg-buildpackage -b -us -uc ||errlog "build deb error"
                mv ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.tar.* $PKGDIR
        fi
}

if [ ! -d "$PKGDIR" ];then
        errlog "$PKGDIR dir is not existd,Exitting !"
fi


if [ -f "$PKGDIR/../autobuild.sh" ];then
        cd $PKGDIR/../
        bash autobuild.sh
else
        cd $PKGDIR
        exec_build

fi
