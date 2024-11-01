#!/bin/bash
errlog(){
        echo $1
        exit 1

}

exec_build(){
	echo "install depends"
	apt update
        yes |mk-build-deps --install --remove
        if [ -f "Makefile" ];then
		echo "clean "
		make clean || echo ok
		echo "build deb in `pwd` "
                make deb
        else
                dpkg-buildpackage -b -us -uc ||errlog "build error"
                cp ../*.deb ../*.buildinfo ../*.changes $PKGDIR
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
