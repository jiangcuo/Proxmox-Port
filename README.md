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
