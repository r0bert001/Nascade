# 脚本说明

## 常用脚本

| 脚本 | 用途 |
| --- | --- |
| `fetch_assets.sh` | 下载中文字体、CA 证书等运行资源 |
| `make_devdata.py` | 生成桌面调试用的模拟 `devdata/`（全部为占位数据） |
| `build_desktop_msys2.sh` | 在 Windows + MSYS2 中构建桌面调试版 |
| `package_stockos.sh` | 把构建目录组装成原厂 `Apps/Nascade.pak` |
| `wsl_bootstrap.sh` | 安装 WSL 构建依赖（仅首次） |
| `wsl_extract_sdk.sh` | 解压 TrimUI SDK、Linaro 工具链和 SDL2 源码 |
| `wsl_build_deps.sh` | 交叉编译 libcurl、libsmb2 |
| `wsl_build_app.sh` | 交叉编译 aarch64 目标程序 |
| `wsl_repack_safe.sh` | 打包、strip、qemu 冒烟测试 |
| `wsl_render_target.sh` | 用 qemu 渲染目标版首页截图 |

在仓库根目录执行 WSL 脚本，例如：

```sh
wsl -d UbuntuBuild1 -u root -- bash scripts/wsl_build_app.sh
wsl -d UbuntuBuild1 -u root -- bash scripts/wsl_repack_safe.sh
```

## 设备调试脚本

这些脚本需要 `sshpass`，并通过 `BRICK_HOST` 指定设备地址，通过 `SSHPASS` 提供密码：

```sh
export BRICK_HOST=192.168.1.50
export SSHPASS='***'
sh scripts/device_install_app.sh
```

| 脚本 | 用途 |
| --- | --- |
| `device_install_app.sh` | 打包并安装到 `/mnt/SDCARD/Apps/Nascade.pak` |
| `device_deploy_tmp.sh` | 上传到设备 `/tmp` 做临时验证 |
| `device_probe_app.sh` | 在设备上测试启动、下载和离屏截图 |
| `device_fetch_shot.sh` | 取回设备截图并转换为 PNG |
| `wsl_ssh_setup.sh` | 在 WSL 中安装 `sshpass` 和 `openssh-client` |

## 诊断/一次性脚本

以下脚本是开发期的诊断工具，默认使用 `/opt/tg5040` 工具链，普通构建和安装不需要它们：

- `wsl_check_pkgconfig.sh`、`wsl_check_stock_abi.sh`
- `wsl_glibc_symbols.sh`、`wsl_inspect_binary.sh`、`wsl_runtime_deps.sh`
- `wsl_isolate_package.sh`、`wsl_package_app.sh`、`wsl_strip_repack.sh`
- `wsl_test_package.sh`、`wsl_test_package_perms.sh`
- `wsl_test_target.sh`、`wsl_test_target_scraper.sh`
- `remote_probe*.sh`
