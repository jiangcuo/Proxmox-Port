# Proxmox Port Kernel

I forked a kernel project:

 `https://github.com/jiangcuo/pve-port-kernel.git`

Currently, there are two architectures: LoongArch64 and ARM64.

The config in the kernel selects all arm64 devices and can theoretically be used for all.

* Please note that if you are using  loongarch64 machine, you need to use a 4k kernel. The 4k kernel is already integrated into the ISO, so this is just a reminder.


## 1. How to install the kernel

### 1.1 Use apt

```bash

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

***Not support!***

## 2. Manual build

See https://github.com/jiangcuo/pve-port-kernel readme.md


