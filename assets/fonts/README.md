# 字体

应用使用 **Noto Sans CJK SC**（SIL Open Font License 1.1）作为中文/日文字体。

- 文件：`NotoSansCJKsc-Regular.otf`
- 来源：<https://github.com/googlefonts/noto-cjk>
- 许可证：<https://openfontlicense.org/open-font-license-official-text/>

该 `.otf` 不提交到仓库（体积约 16MB），可用 `scripts/fetch_assets.sh` 下载。打包到原厂 `Apps/Nascade.pak/assets/fonts/` 时，请同时附带 OFL 许可证文本或链接。

桌面开发时如果缺少该字体，程序会回退到 Windows 的 `msyh.ttc`；真机包必须自带字体。
