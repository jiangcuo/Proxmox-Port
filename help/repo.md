# Proxmox-Port  Repo List

### Mirrors

```bash
Golbal: https://global.mirrors.apqa.cn (Cloudflare)
Korea: https://mirrors.apqa.cn (Cloudflare)
Hong Kong: https://hk.mirrors.apqa.cn (direct)
China: https://mirrors.lierfang.com (direct)
Germany: https://de.mirrors.apqa.cn (direct)
```

### key 

    https://mirrors.apqa.cn/proxmox/debian/pveport.gpg
    
   ```bash
   curl https://mirrors.apqa.cn/proxmox/debian/pveport.gpg -o /etc/apt/trusted.gpg.d/pveport.gpg
```

### pve repo
  
pve7

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pve bullseye port
   ```

pve8

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pve bookworm port
   ```
If you want to use test repo:

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pve bookworm pvetest
   ```
ceph-reef
   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pve bookworm ceph-reef
   ```


loong64  sid only

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pve sid port
   ```

### pbs repo

pve7

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pbs bullseye port
   ```

pve8

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/pbs bookworm port
   ```


### devel

pve7

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/devel bullseye port
   ```

pve8

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/devel bookworm port
   ```

### Kernel Repo

   ```bash
   deb https://mirrors.apqa.cn/proxmox/debian/kernel sid port
   ```
kernel build form https://github.com/jiangcuo/pve-port-kernel and may not compatible all machine
