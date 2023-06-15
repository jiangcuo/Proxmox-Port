# This is prebuild edk2 firmware

## 1. Build RISCV64 
https://github-wiki-see.page/m/riscv-non-isa/riscv-acpi/wiki/PoC-%3A-How-to-build-and-test-ACPI-enabled-kernel

###  a. Clone source code
```bash
git clone --branch riscv_acpi https://github.com/ventanamicro/opensbi.git opensbi
git clone --recurse-submodule git@github.com:tianocore/edk2.git edk2
```
###  b. Build edk2
```bash
export WORKSPACE=`pwd`
export GCC5_RISCV64_PREFIX=/usr/bin/riscv64-linux-gnu-
export PACKAGES_PATH=$WORKSPACE/edk2
export EDK_TOOLS_PATH=$WORKSPACE/edk2/BaseTools
source edk2/edksetup.sh
make -C edk2/BaseTools clean
make -C edk2/BaseTools
make -C edk2/BaseTools/Source/C
source edk2/edksetup.sh BaseTools
build -a RISCV64 --buildtarget RELEASE -p OvmfPkg/RiscVVirt/RiscVVirtQemu.dsc -t GCC5
truncate -s 32M Build/RiscVVirtQemu/RELEASE_GCC5/FV/RISCV_VIRT.fd
```
edk2 firmware will be located at `Build/RiscVVirtQemu/RELEASE_GCC5/FV/RISCV_VIRT.fd`

### c. Build OpenSBI

```bash
cd opensbi
make ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu- PLATFORM=generic
```
sbifw located at `opensbi/build/platform/generic/firmware/fw_dynamic.bin`

## 2. Build Loongarch64
https://mirrors.pku.edu.cn/loongarch/archlinux/images/README.html
