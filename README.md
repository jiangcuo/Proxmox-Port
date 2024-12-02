# Proxmox VE Port package source

## How to Build

### Initializing the build host
We need Debian 12 for this code

```bash
apt update
apt install quilt docker.io -y
cd docker
docker build -t pvebuilder -f dockerfile.arm64 .
```

### Build via Docker

```bash
bash build.sh pve-common
```

The built package will be under /tmp/pve-common


### Custom Docker Image

```bash
BUILDERNAME=dockerpull.com/pvebuilder:20241101 bash build.sh pve-common
```


### proxmox-backup-restore-images

This package will pull in the kernel and ZFS packages, which can cause failures if the network is slow, so an additional `submodule.list` check has been added.

If the file exists, it will use the submodules listed in the file, with the file format referencing the `packages/proxmox-backup-restore-image/autobuild.sh` file.
