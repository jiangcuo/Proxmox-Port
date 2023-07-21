# Proxmox-Port

I plan to port Proxmox VE and Proxmox Backup Server to any architecture.

By now, arm64 is finished. https://github.com/jiangcuo/Proxmox-Arm64

# Progress

* pve6 base on debian sid -> done

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
