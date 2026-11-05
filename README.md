# LogicSim

LogicSim 是一个基于 C++20、wxWidgets 和 Visual Studio 2022 的数字逻辑电路仿真器。

当前仓库是可扩展的项目骨架，已经包含：

- 一个可打开的 wxWidgets 主窗口；
- 一个自定义电路画布；
- 公共电路数据模型；
- 仿真核心、元件库和文件读写模块的边界；
- CMake/vcpkg 构建配置；
- 一个不依赖 wxWidgets 的基础测试程序。

## 环境要求

- Visual Studio 2022，安装“使用 C++ 的桌面开发”；
- CMake 3.24 或更高版本；
- vcpkg；项目会优先读取 `VCPKG_ROOT`，也会自动查找 VS2022 自带版本；
- 网络连接，用于首次安装 wxWidgets 和 nlohmann-json。

Visual Studio 2022 通常已经自带 vcpkg。项目会自动检查下面的路径：

```text
C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg\vcpkg.exe
```

如果使用其他位置，可以设置 `VCPKG_ROOT`：

```powershell
[Environment]::SetEnvironmentVariable(
    "VCPKG_ROOT",
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\vcpkg",
    "User"
)
```

设置后重新启动 Visual Studio。不要把不同版本的 vcpkg 同时混入同一个构建目录。

## 在 Visual Studio 2022 中打开

1. 在 VS2022 中选择“文件” -> “打开” -> “文件夹”。
2. 选择本项目根目录。
3. 选择 CMake preset `vs2022-x64-debug`。
4. 将 `LogicSim` 设为启动目标并运行。

首次配置会从 vcpkg 注册表获取依赖。如果出现
`Failed to connect to github.com:443`，说明 GitHub 暂时不可访问；应先恢复网络、
学校 VPN 或本机代理，再重新配置。不要在克隆失败后执行
`bootstrap-vcpkg.bat`，因为该文件此时尚不存在。

也可以从 Developer PowerShell 构建：

```powershell
cmake --preset vs2022-x64-debug
cmake --build --preset vs2022-x64-debug
ctest --preset vs2022-x64-debug
```

## 目录职责

```text
src/model      公共数据结构，不依赖 wxWidgets
src/simcore    仿真计算，不依赖 wxWidgets
src/components 元件定义和行为
src/editor     wxWidgets 画布和编辑交互
src/fileio     JSON 保存、打开和版本迁移
src/app        主窗口和程序入口
tests          不创建窗口的自动测试
docs           架构和文件格式说明
examples       示例电路
```

## 第一条集成目标

第一周只完成下面的闭环：

> 放置两个输入和 AND 门 -> 连线 -> 连接 LED -> 运行仿真 -> 输入变化后 LED 变化 -> 保存并重新打开。

在闭环跑通前，不增加触发器、子电路或波形功能。
