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

## 运行时防护

- MiniEAP 会先丢弃截断或长度不一致的 EAP/EAPOL 帧，再把有效帧交给数据包插件和认证状态机。
- 第一个受支持的 EAP Request 会确定认证服务器；此后的认证帧必须来自同一源 MAC。重新认证时会清除此绑定，以便重新发现认证服务器。
- 如果配置的日志文件无法打开，MiniEAP 会将错误写入 syslog；前台模式还会同步输出到标准错误。
