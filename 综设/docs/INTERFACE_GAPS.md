# 接口问题与建议改动（B 视角）

以下问题是在实现 `ll_sim` 时实际遇到的、`docs/INTERFACES.md` v1.0-draft
没有完全写死或自相矛盾的地方。B 已经按"不破坏冻结签名、只做加法"的原则
给出临时处理；建议第 1 周评审时逐条拍板，并回写接口文档。

| # | 位置 | 问题 | B 当前处理 | 建议 |
| --- | --- | --- | --- | --- |
| 1 | §7.2 时钟 | 公式 `clock_level = ((tick + 1) / 2) % 2 == 1` 与"每 2 个 tick 一个周期""上升沿在 tick 1,3,5"以及 §14.2"DFF 每两个 tick 翻转一次"矛盾。按公式是 4 tick 周期、上升沿在 1,5,9... | 采用语义描述 + 黄金用例：`clock_level = (tick % 2) == 1`，tick 1,3,5... 上升 | 二选一后改公式或改文字，并把 `tests_sim/clock_test.cpp` 作为唯一裁定；B 的代码只需改一行 |
| 2 | §7.2 / §8.3 | `Engine` 没有 `Stop/Pause/Step/Run/Mode`，但 §8.3、§10.1、§10.5 调用了 `engine.Stop()` | 增加 `Run/Pause/Stop/Step/Mode`（纯新增，不改旧签名） | 把 5 个方法补进 `Engine`；`RunMode` 才有实际消费者 |
| 3 | §6.2 / §7.2 | `EvalContext` 没有"用户输入值"字段，`SetInput` 无法经 `Eval` 传给输入元件 | 增加 `ComponentDef::IsInputSource()`（默认 false）；引擎对这类元件跳过 Eval、用覆盖值直接驱动。类型名 `input/button/...` 兜底 | 正式加入 `IsInputSource()`，C 的 `InputDef/ButtonDef` 覆盖为 true；或者给 `EvalContext` 加 `LogicValue user_value`（后者要改结构，不推荐） |
| 4 | §7.1 | `BuildNetlist(const Document&)` 没有 `ComponentRegistry` 参数，但引脚位置必须由 `ComponentDef::Pins()` 计算；测试/自定义元件无法用 | 增加 `BuildNetlist(const Document&, const ComponentRegistry&)` 重载；单参数版用 `BuiltinComponents()` | 把带 registry 的重载写进接口；A 的高亮仍可用单参数版 |
| 5 | §7.1 / §11 | `BuildNetlist` 返回 `Netlist` 而不是 `Status`，未知类型、子电路环、坏绑定无法报告给调用方；`Engine::Load` 只能自己再校验 | `BuildGraph` 内部返回 `BuildResult{status,...}`；`Engine::Load` 用它；公开 `BuildNetlist` 失败时返回空网表 | 增加 `BuildResult BuildNetlistChecked(...)`，或让 `Netlist` 带 `Status`；A 需要知道高亮为什么为空 |
| 6 | §6.7 / §7.4 | `SubcircuitDef(CircuitId, string)` 只有一个 `Target()`，`Pins()` 却应由目标电路 `Ports()` 生成；构造函数拿不到 `Document`，无法生成引脚。另外 `Port.position` 同时被当作目标电路内部坐标和子电路实例引脚偏移，语义不清 | 给 `SubcircuitDef` 增加 `SetPorts()`；`Port.position` 直接作为实例引脚偏移；B 展开时用目标 `Ports()` 建锚点 | 明确 `Port.position` 的坐标系；建议给 `SubcircuitDef` 增加 `SetPorts(std::vector<Port>)` 或构造时传入 `Document&`；C/D 在创建子电路元件时填好端口 |
| 7 | §5.1 / §5.2 | `Circuit` 只有 `RestorePort`，没有正常的 `AddPort/RemovePort/SetPorts`；新建子电路时无法通过公开接口添加端口 | 参考 model 增加 `AddPort/RemovePort/SetPorts`（加法式） | D 补 `AddPort/RemovePort`；否则 A 的"从选区创建子电路"无法落地 |
| 8 | §7.5 / §12.1 | `const Waveform& GetWaveform() const` 返回 const，A 无法调 `SetWatched()`（非 const） | 增加非 const 重载 `Waveform& GetWaveform()` | 接口加非 const 重载，或把 `SetWatched` 改成 `Engine::SetWatchedNets(...)` |
| 9 | §7.2 / §10.3 | `Advance(n)` 期间 Listener 是"每个 tick 一次"还是"最后一个 tick 一次"？§7.2 说每 tick，§10.3 说最后一个 | 每 tick 调一次（A 可自己合并） | 统一为每 tick 一次，并在 §10.3 修正文字 |
| 10 | §7.3 | `FloatingInput` 与 `FloatingNet` 的边界没写死；只有输出引脚且输出 Z 的网络算哪种？ | 有输入引脚 → `FloatingInput`；无输入引脚 → `FloatingNet`；两者不重复报 | 在文档里写死这条优先级 |
| 11 | §4.1 | "单 bit 驱动多 bit 网络广播到所有位"与 `WidthMismatch` 诊断的边界不清；4 位接 8 位是否报错？ | 只有 1 位是合法适配；其它非 1 位不一致报 `WidthMismatch`，按最大位宽继续 | 在 §4.1/§7.3 明确"1 位是唯一允许的隐式适配" |
| 12 | §7.1 | 连通规则只写了"导线端点落在另一条导线线段中间"，没写"引脚落在导线段中间"和"两个引脚坐标相同" | 都视为连接（物理接触）；见 `SIM_DESIGN.md §2.1` | 如果团队要严格 Logisim 语义，明确只认端点/绑定，B 删除引脚测试点即可 |
| 13 | §5.2 | `RemoveComponent` 说"级联删除吸附其上的导线端点"，是删除整条导线还是仅解除绑定？ | 解除绑定：`attached=false`，端点坐标保留，导线本身不删 | D 明确；建议"解除绑定，不删除导线" |
| 14 | §9.2 | `LoadResult` 按值含 `Document`，但 `Document` 若不可移动则无法返回；拷贝又会复制大对象 | 参考 model 把 `Document` 做成 move-only，并提供移动构造/赋值 | D 在实现 JSON 时确认 `Document` 可移动；`LoadResult` 用 `std::move` |
| 15 | §7.2 | `SimFrame` 在 `error.h` 定义 `NetlistError` 之前就引用它；单独 include `engine.h` 可能编译不过 | `engine.h` 先 include `error.h`；`error.h` include `netlist.h` | 调整头文件依赖顺序，或把 `NetId` 抽到更基础的 `sim/types.h` |

## 需要 B 之外的人确认的优先级

1. **最高**：第 1 条时钟。它直接决定 DFF/计数器/黄金用例，越晚改越贵。
2. **高**：第 3 条输入值、第 4 条 registry、第 6 条子电路端口。
3. **中**：第 8、9、10、11 条，主要是文档口径。
4. **低**：第 13、14、15 条，实现时顺手处理即可。

B 侧代码对以上问题都做了兼容处理；即使团队决定采用不同口径，改动点也
集中在 `Engine::Impl::Tick`、`Engine::Impl::Settle`、`NetlistBuilder::Expand`
三处，不会扩散到 A/C/D。
