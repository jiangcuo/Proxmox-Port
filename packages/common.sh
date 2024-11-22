#!/bin/bash
copy_dir(){
                rsync -ra $SH_DIR/$PKGNAME /build
                cd /build/$PKGNAME
}

exec_build_make(){
        apt update
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
        if [ $dscflag == "dsc" ];then
                make dsc ||  "dsc build error but it is not  fatal error"
        fi
        DEB_BUILD_OPTIONS=nocheck  make deb || errlog "build deb error"
}

exec_build_dpkg(){
        apt update
        yes |mk-build-deps --install --remove
        echo "clean "
        make clean || echo ok
        echo "build deb in `pwd` "
        dpkg-buildpackage -b -us -uc || errlog "build deb error"
}

errlog(){
   echo $1;
   exit 1;
}
