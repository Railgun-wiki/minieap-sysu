# MiniEAP-SYSU

[minieap](https://github.com/updateing/minieap) 的 SYSU 适配版本。

## Build

> 如果目的是在 OpenWrt 上使用 minieap 认证校园网，建议移步 [openwrt-minieap-sysu](https://github.com/undefined443/openwrt-minieap-sysu)（minieap 主程序）和 [luci-app-minieap](https://github.com/kongfl888/luci-app-minieap)（minieap Web 管理插件），它们对 OpenWrt 提供了专门适配。

1. 下载你路由器型号的 Toolchain：

   [ImmortalWrt Firmware Selector](https://firmware-selector.immortalwrt.org/)

   [OpenWrt Firmware Selector](https://firmware-selector.openwrt.org/)

2. 克隆源码。
3. 修改 `config.mk`，将其中的 `CC` 修改为 Toolchain 中 GCC 编译器的路径。
4. 使用 `make` 编译。

## Usage

```sh
./minieap -u <username> -p <password> -n <nic>
```
