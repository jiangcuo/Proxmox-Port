# Proxmox-Port  Repo List

### Mirrors

```bash
cloudflare: https://global.mirrors.apqa.cn/
Asia: https://mirrors.apqa.cn    //no cdn
```

### key 

    https://global.mirrors.apqa.cn/proxmox/debian/pveport.gpg
    
   ```bash
   curl https://mirrors.apqa.cn/proxmox/debian/pveport.gpg -o /etc/apt/trusted.gpg.d/pveport.gpg
```

### pve repo
  
pve7

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/pve bullseye port
   ```

pve8

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/pve bookworm port
   ```

loong64  sid only

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/pve sid port
   ```

### pbs repo

pve7

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/pbs bullseye port
   ```

pve8

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/pbs bookworm port
   ```


### devel

pve7

   ```bash
   deb https://global.mirrors.apqa.cn/proxmox/debian/devel bullseye port
   ```

pve8

   ```bash
   deb https:global.mirrors.apqa.cn/proxmox/debian/devel bookworm port
   ```


