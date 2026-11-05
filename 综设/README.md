# Logisim-Lite · B 仿真核心

本仓库是课程分组项目 **Logisim-Lite** 中 **B（仿真核心）** 的交付物，按
[`docs/INTERFACES.md`](docs/INTERFACES.md) v1.0-draft 的接口实现。目标是把
`ll/sim` 做到可以直接被 A 的编辑器、C 的元件库和 D 的模型层集成，并且可以独立测试。

> 说明：工作区原本是空目录。为了让仿真核心能够单独编译、跑测试，本仓库同时放了
> `model/`、`component/` 的一套**最小参考实现/脚手架**。这两层分别属于 D 和 C；
> 正式集成时请用他们的实现替换 `.cpp`，公共头文件与 `INTERFACES.md` 保持一致。
> B 的正式交付范围是 `sim/`、`tests/` 中的仿真测试，以及 `docs/SIM_DESIGN.md`。

## 1. 已实现内容

| 周次 | B 的任务 | 对应实现 | 测试 |
| --- | --- | --- | --- |
| 第 1 周 | 逻辑值、仿真快照、事件接口 | `model/logic_value.*` 语义、`SimFrame`、`Listener` | `logic_value_test.cpp` |
| 第 2 周 | AND 简单传播、输入更新 | `Engine::SetInput` / `Tick` / `Settle` | `engine_and_truth_table` |
| 第 3 周 | 网表编译、多引脚、稳定迭代 | `BuildNetlist`、并查集、T 型连接、200 轮 Settle | `netlist_*`、`stress_deep_combinational_chain` |
| 第 4 周 | 悬空、短路/多驱动、组合环诊断 | `NetlistError` + `RefreshDiagnostics` | `diagnostic_*` |
| 第 5 周 | 时序调度、时钟推进 | `CommitEdge` 两段式协议、全局时钟 | `sequential_test.cpp` |
| 第 6 周 | 暂停/单步/复位、确定性、循环上限 | `Run/Pause/Stop/Step`、`Reset`、ID 升序求值 | `engine_run_pause_stop_step_modes` |
| 第 7 周 | 测试向量、波形记录 | `Waveform`、边沿采样、50 万条上限 | `waveform_test.cpp` |
| 第 8 周 | 大电路、性能稳定性 | 空间分桶 T 型连接、复用的求值缓冲 | `stress_independent_gates` |
| 第 9 周 | 回归与核心测试 | 29 个自动化测试 | 全部 |

## 2. 目录

```text
model/                 D 的参考模型（LogicValue 语义由 B 提供）
component/             C 的参考元件库（输入/LED/门/DFF/计数器/MUX/子电路）
sim/include/ll/sim/    B 的公共接口：netlist.h / engine.h / error.h / waveform.h
sim/src/               B 的实现：netlist.cpp / engine.cpp / waveform.cpp
tests/                 B 的测试；test_framework.h 是无依赖测试框架
docs/                  设计说明、周次清单、接口问题清单、测试报告
scripts/               CMake 构建脚本与直接 cl.exe 构建脚本
```

## 3. 构建与测试

### 3.1 推荐：CMake + MSVC 2022

在 “x64 Native Tools Command Prompt for VS 2022” 中：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

或者直接运行脚本（会自动找 VS 2022、vcvars64 和 CMake）：

```powershell
.\scripts\build.ps1
.\scripts\build.ps1 -Configuration Debug
```

如果系统提示“脚本没有数字签名”，用：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

如果机器上 CMake 可用但 VS 不在默认位置，先设置 `VSINSTALLDIR`，或修改
`scripts/build.ps1` 顶部的路径探测。

### 3.2 备选：只调用 cl.exe

没有 CMake、或 CMake 的 `try_compile` 被环境限制时：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_direct.ps1
```

脚本会编译 `model/ component/ sim/ tests/` 下的全部源文件到
`build/manual/`，链接 `ll_tests.exe` 并直接运行。

### 3.3 当前验证结果

- MSVC 19.44 / C++20，`/W4 /permissive-`，无警告。
- `29 tests, 0 failed, 0 checks failed`。
- 500 个独立门 + 100 级 NOT 链：Release 下约 11–13 ms（见测试输出）。

## 4. 给 A / C / D 的集成说明

### A（编辑器）

```cpp
ll::sim::Engine engine(ll::BuiltinComponents());
engine.Load(doc);

engine.SetInput({input_id, 0}, ll::LogicValue::One(1));
engine.Tick();                              // 或 engine.Advance(n)
const ll::LogicValue v = engine.Value({led_id, 0});

engine.SetListener([](const ll::sim::SimFrame& frame) {
    // frame.changes 里是本 tick 变化的引脚；frame.errors 里是诊断
    // frame.converged 为 false 时 UI 可提示"组合环/振荡"
});

// 高亮：不要自己写并查集
const ll::sim::Netlist& net = engine.NetlistRef();
ll::sim::NetId n = net.NetOf({component_id, pin_index});
for (const ll::PinRef& p : net.PinsOf(n)) { /* 同一网络 */ }
```

- 校验/缓存网表可以按 `Document::Revision()` 或 `net.Fingerprint()`。
- 工具栏状态可直接用 `engine.Run()/Pause()/Stop()/Step()/Mode()`；
  也可以忽略这些方法，由 A 自己管理定时器并只调用 `Tick()`。
- `engine.GetWaveform()` 的非 const 重载用于 `SetWatched()`；绘制仍然只读
  `Samples()`。

### C（元件库）

- `Eval` 保持纯函数、只写输出口；时序元件用 `ComponentState` +
  `CommitEdge`，见 `component/src/builtin.cpp` 的 `DffDef`。
- 输入类元件请实现新增的 `IsInputSource() == true`（默认 false）。
  `EvalContext` 没有"用户输入值"字段，B 的引擎对这类元件不调用 `Eval`，
  而是用 `SetInput` 的值直接驱动其引脚。没有这个标记时，B 会按类型名
  `input/button/switch/dip/constant` 兜底。
- `SubcircuitDef::Pins()` 由 `Ports()` 生成；B 在网表阶段展开，不调用
  `Eval()`。

### D（模型 / 集成）

- `Engine::Load` 会先调用 `Document::Validate()`；失败时保留旧网表。
- 撤销后 `Revision++`，A 重新 `Load` 即可。`Engine` 会保留仍存在的输入
  引脚的 `SetInput` 值，并清空波形。
- 本仓库的 `model/` 是参考实现，`Document::Validate()` 只做结构检查；
  子电路环由 B 的 `BuildGraph` 在展开时用 64 层深度上限和调用栈检测。

## 5. 关键约定与需要团队拍板的事项

1. **时钟**：`INTERFACES.md §7.2` 的公式
   `clock_level = ((tick + 1) / 2) % 2 == 1` 与同段的
   “每 2 个 tick 一个周期 / 上升沿在 tick 1,3,5”以及黄金用例
   “DFF 每两个 tick 翻转一次”互相矛盾。B 目前按**语义描述和黄金用例**
   实现：`clock_level = (tick % 2) == 1`，即 tick 1,3,5... 为上升沿。
   代码里只有一个函数 `Engine::Impl::Tick` 负责这件事，若团队最终选择
   公式，改一行即可；见 `docs/INTERFACE_GAPS.md` 第 1 条。
2. **输入值注入**：`EvalContext` 没有输入值字段，`SetInput` 无法通过
   `Eval` 传给元件。B 增加了 `ComponentDef::IsInputSource()`（带默认实现
   的加法式虚函数），引擎对这类元件跳过 `Eval`、直接用 `SetInput` 驱动。
3. **`BuildNetlist` 需要 registry**：原接口只有 `BuildNetlist(const Document&)`，
   但计算引脚位置必须有 `ComponentDef::Pins()`。B 增加了
   `BuildNetlist(const Document&, const ComponentRegistry&)` 重载；单参数
   版本使用 `BuiltinComponents()`。
4. **`Engine` 缺少停止/单步接口**：文档 §8.3/§10.1 调用了不存在的
   `engine.Stop()`。B 增加了 `Run/Pause/Stop/Step/Mode`，不改变原有方法。
5. **位宽**：1 位驱动多位的网络按“广播到所有位”处理；多位驱动 1 位取
   bit0；其它位宽不一致（例如 4 位接 8 位）报 `WidthMismatch`，按最大
   位宽继续仿真。

更多细节见 `docs/SIM_DESIGN.md`；接口问题与建议改动见
`docs/INTERFACE_GAPS.md`。

## 6. 当前不包含的内容

- `editor/`、`app/`、wxWidgets 界面（A 的范围）。
- JSON 序列化、原子保存、迁移（D 的范围）。
- 三态总线/分线器（`INTERFACES.md §1.2` 明确 v1 不做）。
- 工作线程仿真（v1 单线程，`Engine` 不提供锁）。

这些都不影响 B 的 `ll_sim` 被集成：`ll_sim` 只依赖
`ll_component` / `ll_model` 的公共头文件。
