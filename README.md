# Proxmox-Port

This is an unofficial community project where we ported Proxmox VE to non-x86 platforms.

ProxmoxVE acts as upstream and Port acts as downstream. If support is required, it is provided by Port, not Proxmox upstream.

Currently support arm64, loongarch64 two architectures, riscv64 is not fully supported.

We have a large number of Kunpeng server and Loongson server users who report that Proxmox VE Port is able to run stably on their servers and as a production environment.

If you want to support the project, you can feedback the issue, submit code, financial support, and any other means conducive to the development of the project. ProxmoxVE is an excellent piece of software, and we shouldn't limit it to amd64.

If you are using it for the first time and the method of creating virtual machines is different from amd64, do not use seabios and q35 models.

More information can be found on our wiki！

## 1. Tested platform:
- Phytium  (arm64) : Starting with pve8.3, we added a phytium kernel to support the S2500 and S5000 processer
- Rockpi  (arm64) 
- Raspberry Pi  (arm64)
- Amlogic TV box  (arm64)
- Kunpeng  (arm64)
- Ampere  (arm64)
- Apple  (arm64. Since apple M3 and M4，You can install utm on mascos and create a vm from pve iso.Then you create vm on pve by nested virtualization .)
- 3A5000/3A6000/3C5000/3C6000 (loongarch64)
- VisionFive2 (riscv64)
  
Detailed: https://github.com/jiangcuo/Proxmox-Port/wiki/Support-List  


## 2. screen shot 
### arm64 

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/0628e750-e0ff-4b74-a90e-865aac4df8fa">

### riscv64

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/841799fc-85c0-4227-ab12-e999ee66ffd3">


Due to the low performance of RISC-V and the  debians snapshot-mirrors is too slow to me, the process of building has been extremely challenging, which is why I haven't updated the RISC-V packages. If conditions permit, it is recommended to use a snapshot "202310?" release to start the building process.


### loongarch64 

<img width="800" alt="image" src="https://github.com/jiangcuo/Proxmox-Port/assets/49061187/1d01de5a-455c-46d4-9ff1-12ba199a5e66">


##  3. Installation

If you are using server that supports EFI,you can install proxmox-ve with iso.

https://mirrors.apqa.cn/proxmox/isos/

If you are using u-boot device or failed with iso, you can install proxmox-ve from repo.

Head to the wiki page to learn more.

## 4.Source Code

Our source code is hosted in other branches of the repository.

## Star History


[![Star History Chart](https://api.star-history.com/svg?repos=jiangcuo/Proxmox-Arm64,jiangcuo/Proxmox-Port&type=Date)](https://star-history.com/#jiangcuo/Proxmox-Arm64&jiangcuo/Proxmox-Port&Date)

