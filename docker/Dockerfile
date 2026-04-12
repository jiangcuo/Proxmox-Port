ARG SUITE=trixie
FROM pxvirt/debian-base-${SUITE}:latest

ARG SUITE

RUN apt update && apt install -y wget nano curl gnupg ca-certificates apt-utils

RUN echo "deb https://mirrors.lierfang.com/pxcloud/pxvirt ${SUITE} main" >> /etc/apt/sources.list \
&& echo "deb https://mirrors.lierfang.com/pxcloud/devel ${SUITE} main" >> /etc/apt/sources.list

RUN curl -L https://mirrors.lierfang.com/pxcloud/lierfang.gpg -o /etc/apt/trusted.gpg.d/lierfang.gpg

RUN apt update && apt -y install build-* devscripts cargo dh-cargo devscripts rsync debcargo pkg-config debhelper git bison dwarves flex meson equivs asciidoc  bc  cpio dwarves  kmod lz4 quilt xmlto zstd

RUN apt update \
    && DEBIAN_FRONTEND=noninteractive apt-get -y --no-install-recommends install pxvirt exim4|| echo ok

RUN rm -rf /var/lib/dpkg/info/pve-manager.postinst \
    && rm -rf /var/lib/dpkg/info/proxmox-ve.postinst \
    && dpkg --configure -a

ADD ./start.sh /

CMD ["bash","/start.sh"]
