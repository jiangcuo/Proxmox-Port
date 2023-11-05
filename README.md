# Proxmox-Port

I plan to port Proxmox VE and Proxmox Backup Server to any architecture.

By now, arm64 is finished. https://github.com/jiangcuo/Proxmox-Arm64

## 1. Source
The Proxmox-Port code is located in the following repositories.

- [zfsonlinux](https://github.com/jiangcuo/zfsonlinux)
- [pve-qemu](https://github.com/jiangcuo/pve-qemu)
- [pve-lxc-syscalld](https://github.com/jiangcuo/pve-lxc-syscalld)
- [pve-lxcfs](https://github.com/jiangcuo/pve-lxcfs)
- [pve-installer](https://github.com/jiangcuo/pve-installer)
- [pve-edk2-firmware](https://github.com/jiangcuo/pve-edk2-firmware)
- [proxmox-mini-journalreader](https://github.com/jiangcuo/proxmox-mini-journalreader)
- [proxmox-backup](https://github.com/jiangcuo/proxmox-backup)
- [proxmox-backup-restore-image](https://github.com/jiangcuo/proxmox-backup-restore-image)
- [proxmox-backup-qemu](https://github.com/jiangcuo/proxmox-backup-qemu)
- [pve-manager](https://github.com/jiangcuo/pve-manager)
- [qemu-server](https://github.com/jiangcuo/qemu-server)
- [pve-port-kernel](https://github.com/jiangcuo/pve-port-kernel)



Master branch is mirror, checkout branch to arm64 / riscv64 / loongarch64 .You can view and build package.


## 2. Progress

* pve6 base on debian sid(loongarch64) -> done
* pve8 base on debian12(arm64) -> done

## 3. Support 

Mail: jiangcuo@bingsin.com

Issue: https://github.com/jiangcuo/Proxmox-Arm64/issues

Discord: https://discord.gg/ZdbD2gDcnP

Help Page: [help.md](help/helpindex.md)

Arm64 wiki: https://github.com/jiangcuo/Proxmox-Arm64/wiki


## 4. Tested platform:
- Rockpi  (arm64) 
- Raspberry Pi  (arm64)
- Amlogic TV box  (arm64)
- Kunpeng  (arm64)
- FT  (arm64)
- Ampere   (arm64)
- Apple  (arm64,vm only,no kvm support)
- 3A5000 (loongarch64)
- VisionFive2 (riscv64,loongarch64)


## 5. Features

- ramfb support.
- add more pice on vm,so we can hotplug and use more nets disks.(not perfect)
- set gic-version=host.
- tpm

##  6. Version

### pve
- public version 
  - 8.0.3 (arm64)

- testing version 
  - 8.0.6 (arm64)
  - 6.4 (loongarch64,riscv64)

### pbs
- public version = 2.3.1-1 (arm64)
- testing version = 2.3.1-1 (arm64)

##  7. Changelog

[Changelog](changlog.md)

##  8. Installtion

If you are using arm64 server that supports EFI,you can install proxmox-ve with iso.

https://mirrors.apqa.cn/proxmox/isos/

If you are using u-boot device or failed with iso, you can install proxmox-ve from repo.

Head to the wiki page to learn more.

## 9. Passthrough
Hardware passthrough looks good

![ ](https://raw.githubusercontent.com/jiangcuo/Proxmox-Arm64/main/images/pasthrough.png)

##  10. More ?

https://github.com/jiangcuo/Proxmox-Arm64/wiki

# Current difficulties

Proxmox VE 7 and Proxmox Backup Server use rust.

Some crates and pkg don't work on riscv and loongarch64 yet,cause I can't build pve7.

such as:

- Ring: 
  - https://github.com/briansmith/ring/pull/1436 //riscv
  
  - https://github.com/zhaixiaojuan/ring.git   //loong64

  - https://github.com/briansmith/webpki/issues/253 

- Criu: 
https://github.com/checkpoint-restore/criu/issues/1702

I will test the pve6 first.

# Interesting thing

Proxmox VE is really awesome！

Thanks to Proxmox adding support for Arm, now I just need to change code
```patch
- $arch eq 'aarch64'
+ $arch ne 'x86_64'
```
and easy support other architectures.

I sincerely hope that Proxmox can make their products support any platform which has kvm supported !
