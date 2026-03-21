# MiniEAP-SYSU

[updateing/minieap](https://github.com/updateing/minieap) 的 SYSU 适配版本。

## Build

> 如果目的是在 OpenWrt 上使用 minieap 认证校园网，建议移步 [openwrt-minieap-sysu](https://github.com/undefined443/openwrt-minieap-sysu)（MiniEAP IPK 软件包）和 [luci-app-minieap](https://github.com/kongfl888/luci-app-minieap)（MiniEAP LuCI 插件），它们对 OpenWrt 提供了专门适配。

1. 根据路由器型号（model）查找对应的 CPU 平台：[OpenWrt Table of Hardware](https://toh.openwrt.org/)

2. 根据 CPU 平台选择对应的 SDK 镜像：

   - [openwrt/sdk](https://hub.docker.com/r/openwrt/sdk/tags)
   - [immortalwrt/sdk](https://hub.docker.com/r/immortalwrt/sdk/tags)

   ```sh
   IMAGE="docker.io/immortalwrt/sdk:mediatek-filogic-24.10-SNAPSHOT"
   ```

3. 配置 CC 路径：

   ```sh
   TOOLCHAIN=$(docker run --rm $IMAGE sh -c 'realpath $(find staging_dir -name "*openwrt-linux-gcc")')
   sed -i "s|gcc|$TOOLCHAIN|" config.mk
   ```

3. 构建：

   ```sh
   docker run -v "$(pwd):/minieap" -w /minieap -u root --rm $IMAGE make
   ```

## Usage

```sh
./minieap -u <username> -p <password>
```

## Develop

1. 修改 [`.devcontainer/devcontainer.json`](.devcontainer/devcontainer.json) 中的 `image` 字段为适合你的 SDK 镜像：

   ```diff
   -"image": "docker.io/immortalwrt/sdk:mediatek-filogic",
   +"image": "docker.io/immortalwrt/sdk:mediatek-filogic-24.10-SNAPSHOT",
   ```

2. 在 VS Code 中使用 Dev Containers 插件打开本项目。
