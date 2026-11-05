# B 周次交付清单

对应分工表 B 列（第 1–9 周）。状态以本仓库当前代码和测试为准。

| 周次 | 分工表任务 | 状态 | 代码 / 测试 |
| --- | --- | --- | --- |
| 1 | 定义逻辑值、仿真快照、事件接口 | 完成 | `model/include/ll/model/logic_value.h`、`sim/include/ll/sim/engine.h` 的 `SimFrame` / `Listener`；`logic_value_test.cpp` |
| 2 | 完成 AND 门简单传播、输入更新 | 完成 | `Engine::SetInput`、`Tick`、`Settle`；`engine_and_truth_table`、`engine_reset_and_input_defaults` |
| 3 | 网表编译、多引脚连接、稳定状态迭代 | 完成 | `sim/src/netlist.cpp` 并查集 + 空间桶 + 200 轮 Settle；`netlist_*`、`stress_deep_combinational_chain` |
| 4 | 悬空、短路、多驱动、组合环诊断 | 完成 | `NetlistError`、`RefreshDiagnostics`；`diagnostic_*` 四个测试 |
| 5 | 状态型元件的调度接口、时钟推进 | 完成 | `ComponentState` / `CommitEdge` 两段式；`sequential_test.cpp` |
| 6 | 暂停/单步/重置、时序确定性、循环上限 | 完成 | `Run/Pause/Stop/Step/Mode`、`Reset`、200 轮上限、ID 升序；`engine_run_pause_stop_step_modes` |
| 7 | 测试向量执行、波形数据记录 | 完成（波形） | `Waveform`、边沿采样、watch 列表；`waveform_test.cpp`。测试向量由各 `*_test.cpp` 直接覆盖，未额外做 UI 向量格式 |
| 8 | 大型电路测试、性能与稳定性修复 | 完成 | 500 门规模 + 100 级链；空间桶、求值缓冲复用；`stress_test.cpp` |
| 9 | 修复仿真回归问题、增加核心测试 | 完成 | 29 个自动化测试；`docs/TEST_REPORT.md` |

## 逐周可演示项

- 第 2 周末：运行 `engine_and_truth_table`，AND + LED 四种输入组合。
- 第 3 周末：`netlist_t_junction_connects`、`netlist_crossing_without_endpoint_does_not_connect`。
- 第 4 周末：`diagnostic_floating_input`、`diagnostic_multiple_drivers`、`diagnostic_combinational_oscillation`。
- 第 5 周末：`dff_toggles_every_two_ticks`、`counter_mod_10_wraps`。
- 第 6 周末：`engine_run_pause_stop_step_modes`、`clock_levels_and_rising_edges`。
- 第 7 周末：`waveform_records_changes_on_watched_net`、`subcircuit_flattens_and_simulates`。
- 第 8 周末：`stress_independent_gates`（输出耗时）、`stress_deep_combinational_chain`。
- 第 9 周末：全部 29 个测试通过，MSVC `/W4 /permissive-` 零警告。

## 尚未完成 / 需要团队配合

1. **A 的编辑器集成**：本仓库没有 `editor/`、`app/`、wxWidgets 代码。
   A 需要把 `Engine` 接到 `wxTimer`、属性面板、画布高亮上。接口示例见
   `README.md` 第 4 节。
2. **C 的正式元件库**：`component/` 只是本地参考实现。C 替换后需要保证
   `Eval` 契约、`IsInputSource()`、`CommitEdge` 的语义不变。
3. **D 的正式模型/序列化**：`model/` 的 JSON 层没有实现；B 的测试用
   程序化构造的 `Document`，等 D 的 `LoadDocument` 落地后，建议把
   `tests/data/*.json` 黄金用例补上。
4. **时钟定义拍板**：见 `docs/INTERFACE_GAPS.md` 第 1 条。
5. **子电路端口位置约定**：见 `docs/INTERFACE_GAPS.md` 第 6 条。
6. **性能上限**：200 轮 Settle 对逻辑深度 >200 的电路会报振荡；第 8 周
   需要和 A/D 一起决定是否提高上限或改事件驱动。
