# PA 安装包

在 PowerShell 中从项目根目录执行：

```powershell
.\scripts\build_installer.cmd -Version 1.0.0
```

脚本会依次完成 Release 构建、`windeployqt` 依赖部署和 Inno Setup 编译。安装包输出到 `installer-output`。

如果已经完成 Release 构建，可使用：

```powershell
.\scripts\build_installer.cmd -Version 1.0.0 -SkipBuild
```

默认按当前机器配置使用 Qt `D:\Qt\6.11.1\msvc2022_64` 和 Inno Setup `D:\Inno Setup 6`，路径不同时可通过 `-QtBin`、`-InnoSetupDir` 指定。

程序按当前设计将 `params`、`data`、`logs`、`screenshots` 放在安装根目录。安装位置采用当前用户可写的 `%LOCALAPPDATA%\Programs\PA`，升级和卸载不会主动删除这些用户数据目录。
