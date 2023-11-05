# Proxmox Port Kernel

I forked a kernel project:

 `https://github.com/jiangcuo/pve-port-kernel.git`

The pre-build kernel file of this project is stored in the repo:

`deb https://global.mirrors.apqa.cn/proxmox/debian/kernel sid port`

Currently, there are two architectures: LoongArch64 and ARM64.

The config in the kernel selects all arm64 devices and can theoretically be used for all.

## 1. How to install the kernel

### 1.1 Use apt

```bash
#add repo
$ echo "deb https://global.mirrors.apqa.cn/proxmox/debian/kernel sid port" >>  /etc/apt/sources.list.d/pveport.list

#fresh 
$ apt update

#Lists the available kernels
$ apt search pve-kernel

#install latest kernel by metapackage
$ apt install pve-kernel-6.1-generic
```

### 1.2 Use dpkg

download kernel package form project releases or repo, use
`dpkg -i pve-kernel-*` to install.

### 1.3 Boot from new kernel

A PC booted with  should automatically  set the latest kernel as the first boot kernel. You just need reboot to apply.

If you encounter a failure with a new kernel ,reboot to grub, select Advanced, and in the next options, select the previous kernel.


### 1.3 uboot devices

The uboot device will  read the  `/{boot_partition}/extlinux/extlinux.conf` file on the boot.

```bash
# if your boot_partition is /dev/mmcblk0p3
$ mount /dev/mmcblk0p3 /opt
# if pve kernel vmlinuz is vmlinuz-6.1.60-generic
$ cp /boot/vmlinuz-6.1.60-generic /opt
$ sync
```


You can edit your extlinux.conf.
```bash
$ nano /opt/extlinux/extlinux.conf
```
```
default l0
menu title QuartzPro64 Boot Menu
prompt 0
timeout 50

# create a new item with new kernel.
label l0
menu label Boot Linux Kernel SDMMC
linux /vmlinuz-6.1.60-generic
fdt /rk3588-quartzpro64.dtb
append earlycon=uart8250,mmio32,0xfeb50000 console=ttyS2,1500000n8 root=/dev/mmcblk1p5 rw rootwait

# backup old boot item
label 20
menu label Boot Linux Kernel SDMMC Backup
linux /Image
fdt /rk3588-quartzpro64.dtb
append earlycon=uart8250,mmio32,0xfeb50000 console=ttyS2,1500000n8 root=/dev/mmcblk1p5 rw rootwait
```

The kernel package has compiled the DTB file, if you need it, you can go to the "/boot/dtbs/{kernel_version}/" folder.

!!! NOTE

*** Make sure you can access your device using the serial port. If your changes are wrong, you may not be able to boot the system, so you have a serial port that you can debug again. ***



## 2. Manual build

See https://github.com/jiangcuo/pve-port-kernel readme.md


