# B 仿真核心设计说明

本文记录 `ll_sim` 的实现口径、边界裁定和联调注意事项。接口签名以
`docs/INTERFACES.md` 为准；本文只解释 B 侧实现，以及几处接口文档没有写死、
必须由团队确认的地方。

## 1. 分层与数据流

```text
Document (D)  ──BuildGraph──▶  BuildResult
                                  ├─ Netlist          （公开，A 也可用）
                                  ├─ FlatComponent[]  （引擎内部）
                                  └─ PinInfo[]        （引脚位宽/方向/宿主）
                                        │
                                        ▼
                                   Engine::Load
                                        │
          SetInput ──▶ overrides       │
                                        ▼
                                   Tick / Settle
                                        │
                     SimFrame(changes, errors, converged)
                                        │
                                        ▼
                                   Listener (A)
```

- `model` 是唯一数据源；`Netlist`、`net_values`、`ComponentState` 都是派生数据。
- `sim` 只 include `ll/model`、`ll/component` 的公共头文件，不碰 `wx/`。
- `BuildGraph` 是网表 + 扁平元件表的唯一构建入口；`BuildNetlist` 只是它的
  公开薄封装。

## 2. 网表编译

### 2.1 节点与并查集

每个电路实例化时分配一个 `space`，避免子电路内部坐标与外层坐标互相吸附。
在同一个 `space` 内，并查集节点包括：

- 每个元件的每个引脚；
- 每条导线一个节点（导线本身是等电位体）；
- 子电路端口锚点。

合并规则：

1. 导线端点 `attached == true` 时，与 `PinRef` 对应的引脚节点合并。
   绑定优先于坐标：即使模型里的坐标暂时没跟上，也按绑定连通。
2. 同一 `space` 内坐标完全相同的节点合并。
3. 导线端点落在另一条导线的线段中间（含折点）时合并，即 T 型连接。
4. 两条导线在中间交叉、但交叉处没有任何端点/折点时**不**合并。
5. 引脚落在导线段中间时也合并（物理上导线碰到了引脚）。这条比接口文档
   第 7.1 节列出的规则更宽一点，联调时如果团队要求严格按"只认端点"，删除
   `LinkTJunctions()` 中的引脚测试点即可。

T 型连接检查使用 64 个半格单元的桶索引，避免大电路上"所有点 × 所有线段"
的平方级扫描；每个点只检查同桶内的线段。

### 2.2 网络编号

- 只有至少包含一个引脚的并查集根才会成为 `NetId`。
- 每个网络内的 `PinRef` 按 `PinKey` 排序；网络之间按最小 `PinRef` 排序，
  因此同一文档两次构建得到相同的 `NetId` 和 `Fingerprint()`。
- `Fingerprint` 是 FNV-1a(64)，混合网络数、每个网络的引脚数、每个
  `PinRef`。它只反映**连通结构**，不包含元件坐标/属性；A 用它做高亮缓存
  是安全的。

### 2.3 子电路展开

- `IsSubcircuit()` 的元件不生成 `FlatComponent`，也不会被 `Eval`。
- 对子电路实例的每个引脚创建一个"边界引脚"节点，并与目标电路对应
  `Port.position` 的锚点合并。目标电路内部的导线端点/引脚只要落在该
  坐标上，就自然并入同一网络。
- 子电路内部元件的 `ComponentId` 重新编号，从"所有电路中最大
  ComponentId + 1"开始；边界引脚保留实例的扁平 ID。这样内部 ID 不会与
  顶层 ID 冲突，编辑器也无法通过 `Value()` 访问内部引脚。
- 递归深度上限 64；用 `CircuitId` 调用栈检测环，命中返回
  `StatusCode::CycleDetected`。
- 端口位置约定：`Port.position` 同时用作子电路实例的引脚偏移。这是
  `INTERFACES.md` 没有写清楚的地方，见 `docs/INTERFACE_GAPS.md` 第 6 条。

## 3. 求值与稳定化

### 3.1 `Tick()` 顺序

```text
1. ++tick
   clock_level = (tick % 2) == 1
   rising      = clock_level && !old_clock_level
2. Settle(rising)
3. if rising: 对所有 IsSequential() 元件 CommitEdge()
4. if rising: Settle(false)   // 传播 Q 的新值
5. 重算 pin_values
6. changes = 与上一 tick 末尾不同的引脚
7. 记录波形（若开启）
8. RefreshDiagnostics()
9. 构造 SimFrame 调 Listener（无变化也调）
```

第 2 步和第 4 步都只做组合迭代；`CommitEdge` 不在 `Eval` 里发生，符合
"Eval 只读 state、CommitEdge 写 state"的两段式协议。

### 3.2 `Settle`

一次迭代：

1. 按 `ComponentId` 升序（`std::sort` 后的扁平元件表）评估所有元件。
2. 每个元件的 `EvalContext.pins` 来自当前网络值，按引脚位宽适配。
3. `EvalResult.pins` 初值是当前引脚值；元件只应写输出位，但即使漏写，
   也不会读到未初始化内存。
4. 收集所有驱动源，按网络合并。
5. 与上一轮网络值比较；无变化即收敛。

驱动源包括：

- 方向为 `Output` / `InOut` 的引脚；
- `IsClockSource()` 元件的输出（引擎直接写 `clock_level`，不调用 Eval）；
- `IsInputSource()` 元件的引脚（引擎用 `SetInput` 值驱动，不调用 Eval）；
- `SetInput()` 对任意引脚设置的覆盖值（作为该引脚所在网络的额外驱动）。

### 3.3 多驱动合并

初始网络值为 `HighZ(width)`。逐个驱动值按位合并：

| 当前累加值 | 新驱动 | 结果 |
| --- | --- | --- |
| Z | 任意 | 新驱动 |
| 已知 0/1 | Z | 保持 |
| 已知 0 | 已知 0 | 0 |
| 已知 0 | 已知 1 | X，标记冲突 |
| 任意 X | 任意非 Z | X |
| 全部驱动都是 Z | — | Z |

- 位宽不一致时按第 4 节适配。
- 只要出现"0 对 1"的冲突就记 `MultipleDrivers`；`X` 对已知值不会额外
  报多驱动，因为输出已经是 X。
- 所有驱动都是 Z 时视为"无有效驱动"，用于悬空诊断。

### 3.4 迭代上限与振荡

- 上限 200 轮，与接口文档一致。
- 超限时把**最后一轮仍在变化**的网络置为 `Unknown(width)`，记一条
  `Oscillation`，`converged = false`，本 tick 正常返回，不阻塞 UI。
- 振荡错误按 `(kind, nets)` 去重，不会每 tick 刷屏。

### 3.5 确定性

- 元件按 `ComponentId` 升序求值；子电路内部 ID 由展开顺序决定，同一文档
  同一构建顺序结果固定。
- 网络编号按最小 `PinRef` 排序，不依赖 `unordered_map` 的遍历顺序。
- `SetInput` 的覆盖值保存在哈希表中，但合并满足交换律；不同驱动源的
  遍历顺序不影响最终值，也不影响冲突标记。
- `changes` 的顺序按 `PinInfo` 顺序，固定。
- 测试 `netlist_fingerprint_is_stable...` 和 `dff_toggles...` 锁定这两点。

## 4. 位宽规则

1. 一个网络的位宽 = 该网络所有引脚位宽的最大值。
2. 1 位驱动多位网络：广播到所有位（`0x1 -> 0xF`，`X -> 全 X`，
   `Z -> 全 Z`）。
3. 多位驱动 1 位网络：取 bit0。
4. 其它情况（例如 4 位接 8 位）：按最大位宽零扩展/截断继续仿真，并报
   `WidthMismatch`。
5. `Engine::Value(PinRef)` 返回网络值适配到该引脚位宽后的结果；1 位网络
   接多位引脚时同样广播，保证"单 bit 驱动多 bit"的语义在引脚侧一致。
6. `LogicValue` 的 `width` 始终在 1..64；所有工厂/适配函数都会 clamp。

## 5. 输入、时钟与状态

### 5.1 输入元件

`EvalContext` 没有"用户输入值"字段，因此 B 的做法是：

- `ComponentDef::IsInputSource()` 为 true 的元件（新增的加法式虚函数，
  默认 false；`input`/`button` 已覆盖）不调用 `Eval`；
- 引擎把 `SetInput` 的值直接驱动到该元件的引脚；
- 没有显式 `SetInput` 时，默认值为 0（属性 `value` 为 bool/整数时读取它）；
- `Load` 时保留仍存在的输入引脚的值；`Reset` 时全部置 0。

类型名 `input/button/switch/dip/constant` 作为兜底识别，保证 C 还没实现
新虚函数时也能工作。

### 5.2 时钟

- 全局单时钟，周期 2 个 tick。
- `clock_level = (tick % 2) == 1`：tick 1,3,5... 高电平并产生上升沿。
- `IsClockSource()` 元件的所有输出引脚由引擎写入 `clock_level`（多位时
  广播），不调用 `Eval`。
- 为什么不是接口文档里的公式：见 `docs/INTERFACE_GAPS.md` 第 1 条。
  代码只在一个地方计算，团队拍板后改一行。

### 5.3 时序元件

- `CreateState()` 在 `Load` 时调用一次；`Reset()` 在 `Load`/`Reset` 时调用。
- `Eval` 读旧 state 输出 Q/Q'；上升沿时引擎先 `Settle`，再对所有
  `IsSequential()` 元件调用 `CommitEdge`，然后再 `Settle` 一次。
- DFF 的 Q' 用 `LogicNot` 保持位宽；计数器用 `bits`/`modulus` 属性。
- `CommitEdge` 的 `EvalContext.clock_rising` 为 true；第二次 `Settle` 的
  `clock_rising` 为 false。

## 6. 诊断

`NetlistError::Kind`：

| Kind | 触发条件 | 说明 |
| --- | --- | --- |
| `MultipleDrivers` | 同一网络出现 0/1 冲突 | 对应"短路/多驱动" |
| `FloatingInput` | 有输入引脚、但没有有效驱动 | 悬空输入 |
| `Oscillation` | 200 轮未收敛 | 组合环/振荡 |
| `WidthMismatch` | 网络内非 1 位引脚位宽不一致 | 按最大位宽继续 |
| `FloatingNet` | 网络没有有效驱动，且没有输入引脚 | 仅提示 |

- 诊断在每次 `Settle` 后、`SimFrame` 构造前刷新。
- 同一 `(kind, 排序后的 nets)` 只报一次；`Load`/`Reset`/`ClearErrors()`
  清空去重表。
- 错误不抛异常、不中断仿真；A 决定是状态栏、日志还是弹窗。
- 子电路边界引脚不参与"输入引脚"统计，避免把端口误报为悬空。

## 7. 波形

- `SetRecording(true)` 后，每个 tick 对**值发生变化**的网络采样一次
  （边沿存储），A 绘制时做阶梯插值。
- `SetWatched({})` 表示记录全部；非空时只记录列表中的 `NetId`。
- 上限 500000 条；超出时丢弃最旧的 25%。
- `Load`/`Reset` 清空采样，但保留 watched 列表。
- `GetWaveform()` 提供 const 和非 const 两个重载；A 用非 const 调
  `SetWatched`，绘制时用 const 读 `Samples()`。

## 8. 性能

- 求值缓冲 `eval_in/eval_out` 复用，避免每个元件每次迭代分配 vector。
- 网表 T 型连接用空间桶，避免 O(引脚 × 线段)。
- 网络驱动合并是 O(网络数 × 位宽)；v1 位宽 ≤64。
- 测试：500 个独立 NOT 门 + 100 级 NOT 链，Release 约 11 ms。
- 深组合链的收敛轮数与逻辑深度同阶，受 200 轮上限约束；这是接口文档
  规定的行为，不是实现缺陷。第 8 周如果真实电路深度超过 200，需要团队
  决定是提高上限还是改成事件驱动队列。

## 9. 测试清单

| 文件 | 覆盖 |
| --- | --- |
| `logic_value_test.cpp` | 工厂、位段、扩展/截断、三值门 |
| `netlist_test.cpp` | AND→LED 三网络、T 型、交叉不连、指纹 |
| `engine_test.cpp` | 真值表、Reset、NetValue、Listener、Run/Pause/Step |
| `sequential_test.cpp` | 时钟电平、DFF 翻转、计数器 0→9、寄存器、时钟元件 |
| `diagnostic_test.cpp` | 悬空输入、多驱动、振荡、位宽不一致 |
| `waveform_test.cpp` | 边沿采样、watch 列表、Clear |
| `subcircuit_test.cpp` | 展开、边界引脚、内部 NOT、递归拒绝 |
| `stress_test.cpp` | 500 门规模、100 级组合链 |

运行：

```powershell
.\build\ll_tests.exe
# 或
ctest --test-dir build --output-on-failure
```
