# PA 打包与升级操作手册

## 1. 文档目的

本文用于指导 PA 项目完成以下工作：

- 首次生成 Windows 安装包；
- 代码修改后重新构建并发布升级安装包；
- 检查发布文件是否完整；
- 保留用户参数、检测数据、日志和截图；
- 处理构建及打包过程中的常见问题。

当前发布方式为：

```text
PA 源码
  → MSBuild 编译 Release/x64
  → windeployqt 收集 Qt 运行依赖
  → Inno Setup 生成安装程序
```

## 2. 当前打包配置

| 项目 | 当前配置 |
| --- | --- |
| 程序名称 | PA System |
| 主程序 | `PA.exe` |
| 架构 | Windows x64 |
| Qt | `D:\Qt\6.11.1\msvc2022_64` |
| Inno Setup | `D:\Inno Setup 6` |
| 安装位置 | `%LOCALAPPDATA%\Programs\PA` |
| 安装包输出目录 | `installer-output` |
| 发布文件暂存目录 | `dist\PA` |
| 程序及快捷方式图标 | `resources\logo.ico` |

选择 `%LOCALAPPDATA%\Programs\PA` 是因为 PA 会在程序根目录写入参数、数据、日志和截图。该位置对当前用户可写，无须管理员权限，也不会受到 `Program Files` 写权限限制。

## 3. 打包环境要求

打包计算机需要安装：

1. Visual Studio，并安装“使用 C++ 的桌面开发”工作负载；
2. Qt 6.11.1 MSVC 2022 64 位版本；
3. Qt VS Tools；
4. Inno Setup 6；
5. 项目能够正常完成 `Release/x64` 构建。

关键工具应存在：

```text
D:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe
D:\Inno Setup 6\ISCC.exe
```

如果实际安装路径不同，可在打包命令中使用 `-QtBin` 和 `-InnoSetupDir` 指定。

## 4. 首次打包步骤

### 4.1 检查代码和资源

确认以下文件存在：

```text
PA.vcxproj
resources\logo.ico
resources\app.rc
resources\app_resources.qrc
installer\PA.iss
scripts\build_installer.ps1
scripts\build_installer.cmd
```

其中：

- `app.rc` 将 `logo.ico` 写入 `PA.exe`，用于资源管理器和快捷方式图标；
- `app_resources.qrc` 将图标写入 Qt 资源，用于程序窗口和任务栏图标；
- `PA.iss` 定义安装目录、快捷方式、升级标识和用户数据保留策略。

### 4.2 本地功能验证

打包前先使用 Visual Studio 运行并检查：

- 程序可以正常启动和退出；
- 可以连接仪器；
- 仪器参数初始化正常；
- 相控阵成像法则生成正常；
- 波形和图像数据接收正常；
- 参数保存和读取正常；
- `logs` 中能够生成日志；
- 窗口、任务栏和 EXE 显示正确图标。

### 4.3 执行一键打包

在项目根目录打开 PowerShell，执行：

```powershell
.\scripts\build_installer.cmd -Version 1.0.0
```

脚本自动完成四个阶段：

1. 调用 MSBuild 编译 `Release/x64`；
2. 清理并重新生成 `dist\PA`；
3. 调用 `windeployqt` 收集 Qt DLL 和插件，并检查关键依赖；
4. 调用 Inno Setup 生成安装程序。

成功后生成：

```text
installer-output\PA-Setup-1.0.0-x64.exe
```

### 4.4 已构建时快速打包

如果确认 `x64\Release\PA.exe` 已经是本次源码生成的最新程序，可以执行：

```powershell
.\scripts\build_installer.cmd -Version 1.0.0 -SkipBuild
```

正式发布通常不建议使用 `-SkipBuild`。如果 Release 程序不是最新的，会把旧代码打进新版本安装包。

### 4.5 自定义工具路径

```powershell
.\scripts\build_installer.cmd `
  -Version 1.0.0 `
  -QtBin "E:\Qt\6.11.1\msvc2022_64\bin" `
  -InnoSetupDir "E:\Inno Setup 6"
```

## 5. 代码修改后的升级打包

每次代码修改后，需要重新编译并生成新的安装包，不能只修改安装包版本号。

### 5.1 确定新版本号

建议使用语义化版本：

| 修改类型 | 示例 | 说明 |
| --- | --- | --- |
| 缺陷修复 | `1.0.0 → 1.0.1` | 修复问题，功能基本不变 |
| 功能增加 | `1.0.1 → 1.1.0` | 增加兼容功能或模块 |
| 不兼容改版 | `1.1.0 → 2.0.0` | 参数结构或操作方式存在重大变化 |

版本号必须递增，不要重复发布内容不同但版本号相同的安装包。

### 5.2 完成代码修改和测试

在生成升级包之前：

1. 保存全部源码；
2. 完成 Debug 测试；
3. 完成 Release 构建测试；
4. 检查参数兼容性；
5. 检查通信、成像、数据保存和日志；
6. 关闭正在运行的 PA 程序。

### 5.3 生成升级安装包

例如从 1.0.0 升级到 1.0.1：

```powershell
.\scripts\build_installer.cmd -Version 1.0.1
```

输出文件为：

```text
installer-output\PA-Setup-1.0.1-x64.exe
```

### 5.4 升级安装方式

用户无需先卸载旧版本，直接运行新安装包并安装到默认的原目录即可。

安装器通过固定的 `AppId` 将新版识别为同一个 PA 产品：

```text
{69E9FB38-FA37-4ED0-B911-ED7BF1552A91}
```

禁止随意修改 `installer\PA.iss` 中的 `AppId`。修改后 Windows 会将其识别为另一个软件，可能造成新旧版本同时存在。

升级时：

- `PA.exe` 会被新版覆盖；
- Qt DLL 和插件会被新版覆盖；
- 开始菜单和桌面快捷方式继续有效；
- Windows 应用列表中的版本号会更新；
- 用户数据目录不会被主动删除。

## 6. 用户数据保留策略

PA 当前在安装根目录使用以下目录：

```text
PA
├─ params
├─ data
├─ logs
└─ screenshots
```

用途如下：

| 目录 | 内容 | 升级策略 |
| --- | --- | --- |
| `params` | 用户参数和默认参数 | 已有文件不覆盖 |
| `data` | 波形、图像或检测数据 | 保留 |
| `logs` | 程序及通信日志 | 保留 |
| `screenshots` | 用户截图 | 保留 |

安装脚本使用 `onlyifdoesntexist` 复制默认参数，因此安装目录已经存在同名参数文件时不会覆盖。

注意：如果代码修改了参数 JSON 的字段名称、类型、层次或含义，安装器不会自动完成数据迁移。程序代码应做到以下至少一项：

- 兼容读取旧参数；
- 按参数版本号执行迁移；
- 迁移前备份原参数；
- 无法兼容时给出明确提示，禁止静默丢弃用户参数。

建议在参数文件中增加类似字段：

```json
{
  "schemaVersion": 1
}
```

## 7. 发布前检查清单

### 7.1 文件检查

- [ ] `x64\Release\PA.exe` 的修改时间对应本次构建；
- [ ] `dist\PA\PA.exe` 是最新版本；
- [ ] `dist\PA\Qt6Core.dll` 等 Qt DLL 存在；
- [ ] `dist\PA\platforms\qwindows.dll` 存在；
- [ ] `dist\PA\params\default.json` 内容正确；
- [ ] `logo.ico` 在 EXE、窗口和安装程序中显示正常；
- [ ] 安装包文件名和版本号正确。

### 7.2 新安装测试

最好在未安装 PA 的测试账户或测试计算机上验证：

1. 运行安装包；
2. 检查默认安装目录；
3. 创建桌面快捷方式；
4. 从桌面快捷方式启动；
5. 检查程序图标；
6. 检查仪器连接和数据接收；
7. 检查四个用户数据目录；
8. 检查日志能够写入安装根目录的 `logs`。

### 7.3 覆盖升级测试

1. 安装上一正式版本；
2. 创建或修改参数文件；
3. 产生一份数据、日志和截图；
4. 直接运行新版安装包覆盖安装；
5. 确认新版程序启动正常；
6. 确认旧参数、数据、日志和截图仍然存在；
7. 确认旧参数可以正常读取；
8. 确认 Windows 应用列表显示新版本号。

### 7.4 卸载测试

确认程序文件可以正常卸载，同时按照当前产品策略保留用户数据目录。若将来需要提供“彻底删除用户数据”功能，应由用户明确选择，不能在普通升级过程中自动删除。

## 8. 常见问题

### 8.1 找不到 MSBuild.exe

确认 Visual Studio 已安装“使用 C++ 的桌面开发”。也可以从 Visual Studio Developer PowerShell 执行打包命令。

### 8.2 提示没有为 Release/x64 配置 Qt

检查 Visual Studio 项目属性：

```text
项目属性
  → Configuration Properties
  → Qt Project Settings
  → Qt Installation
```

确保 `Release/x64` 指向 Qt 6.11.1 MSVC 64 位版本。打包脚本也会通过 `-QtBin` 推导 Qt 安装目录传递给 MSBuild。

### 8.3 找不到 windeployqt.exe

指定正确路径：

```powershell
.\scripts\build_installer.cmd -Version 1.0.1 `
  -QtBin "D:\Qt\6.11.1\msvc2022_64\bin"
```

### 8.4 找不到 ISCC.exe

指定 Inno Setup 安装目录：

```powershell
.\scripts\build_installer.cmd -Version 1.0.1 `
  -InnoSetupDir "D:\Inno Setup 6"
```

### 8.5 安装后程序无法启动并提示 Qt 平台插件错误

检查：

```text
安装目录\platforms\qwindows.dll
```

禁止只把 `PA.exe` 单独交付给用户。应始终交付完整安装包。

### 8.6 安装器提示缺少 dxcompiler.dll 或 dxil.dll

Inno Setup 的依赖扫描可能提示未找到 Direct3D 12 的可选组件。当前 PA 如果未使用依赖这些 DLL 的 Direct3D 12 功能，该警告不影响 Qt Widgets 程序安装和运行。若后续启用相关图形功能，需要重新评估并部署对应组件。

### 8.7 新版代码已经修改，但安装后仍是旧功能

常见原因是使用了 `-SkipBuild`，或者 `x64\Release\PA.exe` 不是最新程序。重新执行不带 `-SkipBuild` 的命令：

```powershell
.\scripts\build_installer.cmd -Version 1.0.2
```

## 9. 大版本升级注意事项

以下情况不能只依靠覆盖文件完成升级，需要额外开发迁移逻辑：

- 参数 JSON 结构发生不兼容变化；
- 数据文件格式发生变化；
- 日志或数据目录位置改变；
- 通信协议和仪器固件版本强绑定；
- 新版废弃了旧 DLL、插件或配置文件；
- 安装目录策略发生变化。

如果某个旧文件必须删除，可在 Inno Setup 的 `[InstallDelete]` 中精确列出文件。不要对 `params`、`data`、`logs`、`screenshots` 使用通配递归删除。

## 10. 正式发布建议

- 保存每个正式版本的源码提交或 Git 标签；
- 保存最终安装包及对应版本说明；
- 记录安装包 SHA-256；
- 对 `PA.exe` 和安装包进行代码签名，避免显示“未知发布者”；
- 使用干净测试环境执行新安装和覆盖升级测试；
- 在发布说明中列出功能变更、缺陷修复、参数兼容性和仪器固件要求。

推荐发布目录结构：

```text
Release-1.0.1
├─ PA-Setup-1.0.1-x64.exe
├─ SHA256.txt
└─ ReleaseNotes-1.0.1.md
```

## 11. 快速命令汇总

正常构建并打包：

```powershell
.\scripts\build_installer.cmd -Version 1.0.1
```

确认 Release 已是最新时跳过构建：

```powershell
.\scripts\build_installer.cmd -Version 1.0.1 -SkipBuild
```

指定 Qt 和 Inno Setup：

```powershell
.\scripts\build_installer.cmd `
  -Version 1.0.1 `
  -QtBin "D:\Qt\6.11.1\msvc2022_64\bin" `
  -InnoSetupDir "D:\Inno Setup 6"
```

最终安装包位置：

```text
installer-output\PA-Setup-<版本号>-x64.exe
```
