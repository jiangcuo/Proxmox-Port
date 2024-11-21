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
        if [ -f "Makefile" ];then
		echo "clean "
		make clean || echo ok
		echo "build deb in `pwd` "
                make deb || errlog "build  deb error"
		# We need copy deb files first beacuse of deb will be clean when dsc build 
		cp ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.tar.* $PKGDIR
		if [ $dscflag == "dsc" ];then
			make dsc || errlog "build  dsc error"
			cp ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.tar.* $PKGDIR
		fi
        else
                dpkg-buildpackage -b -us -uc -S -d ||errlog "build  dsc error"
		# We need copy deb files first beacuse of deb will be clean when dsc build
                cp ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.tar.* $PKGDIR
                if [ $dscflag == "dsc" ];then
			dpkg-buildpackage -b -us -uc ||errlog "build deb error"
	                cp ../*.deb ../*.buildinfo ../*.changes ../*.dsc ../*.tar.* $PKGDIR
		fi
        fi
}

if [ ! -d "$PKGDIR" ];then
        errlog "$PKGDIR dir is not existd,Exitting !"
fi


if [ -f "$PKGDIR/../autobuild.sh" ];then
        cd $PKGDIR/../
        bash autobuild.sh
else
        # Rust not need copy
        if [ ! -f "Cargo.toml" ];then
                cd $PKGDIR
                exec_build
        else
                mkdir /build/
                rsync -ra $PKGDIR /build
                cd /build/data
                exec_build
        fi


fi
