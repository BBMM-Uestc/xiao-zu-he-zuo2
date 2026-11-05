# 测试报告（B 仿真核心）

## 环境

| 项 | 值 |
| --- | --- |
| 日期 | 2026-11-05 |
| 编译器 | MSVC 19.44.35228（Visual Studio 2022，x64） |
| 标准 | C++20 |
| 编译选项 | `/W4 /permissive- /utf-8 /EHsc` |
| 构建方式 | CMake 3.31.6 / Ninja Release，以及直接 `cl.exe` 两种方式均已验证 |
| 测试框架 | `tests/test_framework.h`（无外部依赖） |

CMake 路径（完整构建 + ctest，已通过）：

```text
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.17 sec
```

最终 29 项测试由 `scripts/build_direct.ps1` 复跑确认。

## 结果

```text
29 tests, 0 failed, 0 checks failed
```

测试列表：

| # | 测试 | 覆盖点 |
| --- | --- | --- |
| 1 | `logic_value_factories` | Zero/One/Unknown/HighZ 掩码 |
| 2 | `logic_value_bits_and_widths` | Bit、Bits、Zext、Trunc、广播/截断 |
| 3 | `logic_value_three_state_gates` | 三值 AND/OR/NOT/XOR |
| 4 | `logic_value_multi_bit_gates` | 多位按位运算 |
| 5 | `netlist_and_led_has_three_nets` | AND + LED 三个网络、引脚归属 |
| 6 | `netlist_t_junction_connects` | 导线端点落在另一条导线中间 |
| 7 | `netlist_crossing_without_endpoint_does_not_connect` | 交叉不连接 |
| 8 | `netlist_fingerprint_is_stable_and_sensitive_to_structure` | 指纹稳定/结构敏感 |
| 9 | `engine_and_truth_table` | AND 00/01/10/11 |
| 10 | `engine_reset_and_input_defaults` | Reset 后输入归零 |
| 11 | `engine_net_value_matches_pin_value` | `NetValue` 与 `Value` 一致 |
| 12 | `engine_listener_fires_once_per_tick` | Listener 每 tick 一次 |
| 13 | `engine_run_pause_stop_step_modes` | Run/Pause/Stop/Step |
| 14 | `clock_levels_and_rising_edges` | 时钟 0/1/0/1 |
| 15 | `dff_toggles_every_two_ticks` | DFF Q' 反馈翻转 |
| 16 | `counter_mod_10_wraps` | 计数器 0→9→0 |
| 17 | `register_samples_on_rising_edge_only` | 上升沿采样 |
| 18 | `clock_component_follows_global_clock` | 时钟元件输出 |
| 19 | `diagnostic_floating_input` | 悬空输入 |
| 20 | `diagnostic_multiple_drivers` | 0/1 冲突 → X + 错误 |
| 21 | `diagnostic_combinational_oscillation` | 200 轮上限 + 振荡错误 |
| 22 | `diagnostic_width_mismatch` | 4 位接 8 位 |
| 23 | `waveform_records_changes_on_watched_net` | 边沿采样、watch |
| 24 | `waveform_clear_keeps_watch_list` | Clear 保留 watch |
| 25 | `subcircuit_flattens_and_simulates` | 子电路展开 + 边界引脚 |
| 26 | `subcircuit_cycle_is_rejected` | 递归子电路拒绝 |
| 27 | `stress_independent_gates` | 500 门，约 11 ms |
| 28 | `stress_deep_combinational_chain` | 100 级 NOT 链收敛 |
| 29 | `engine_reset_clears_attr_initialized_input` | Reset 把带 `value` 初值的输入归零 |

## 复现

```powershell
.\scripts\build_direct.ps1
```

或：

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## 已知限制

- 200 轮 Settle 上限对逻辑深度 > 200 的组合链会报 `Oscillation`；这是接口
  文档规定的行为。如果真实电路需要更深，团队需决定提高上限或改事件驱动。
- 子电路端口位置语义依赖 `docs/INTERFACE_GAPS.md` 第 6 条的约定。
- `model/`、`component/` 是参考实现，JSON/编辑器不在本仓库测试范围。
