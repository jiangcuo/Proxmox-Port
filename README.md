# Proxmox-Port

Arm64/~~Riscv64~~/Loongarch64 are ok. 

There is a detailed introduction in the wiki.

## 1. Tested platform:
- Phytium  (arm64) : Starting with pve8.3, we added a phytium kernel to support the S2500 and S5000 processer
- Rockpi  (arm64) 
- Raspberry Pi  (arm64)
- Amlogic TV box  (arm64)
- Kunpeng  (arm64)
- Ampere  (arm64)
- Apple  (arm64,vm only,no kvm support)
- 3A5000/3A6000/3C5000/3C6000 (loongarch64)
- VisionFive2 (riscv64)
  
Detailed: https://github.com/jiangcuo/Proxmox-Port/wiki/Support-List  


## 2. screen shot 
### arm64 

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/0628e750-e0ff-4b74-a90e-865aac4df8fa">

### riscv64

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/841799fc-85c0-4227-ab12-e999ee66ffd3">



Due to the low performance of RISC-V and the debians snapshot-mirrors is too slow to me, the process of building has been extremely challenging, which is why I haven't updated the RISC-V packages. If conditions permit, it is recommended to use a snapshot "202310?" release to start the building process.


### loongarch64 

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/1d01de5a-455c-46d4-9ff1-12ba199a5e66">


##  3. Installation

If you are using server that supports EFI, you can install proxmox-ve with iso.

https://mirrors.apqa.cn/proxmox/isos/

If you are using u-boot device or failed with iso, you can install proxmox-ve from repo.

Head to the wiki page to learn more.


## Star History


[![Star History Chart](https://api.star-history.com/svg?repos=jiangcuo/Proxmox-Arm64,jiangcuo/Proxmox-Port&type=Date)](https://star-history.com/#jiangcuo/Proxmox-Arm64&jiangcuo/Proxmox-Port&Date)

