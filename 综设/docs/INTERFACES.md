# Logisim-Lite 模块接口文档

| 项 | 内容 |
| --- | --- |
| 文档版本 | v1.0（草案，待第 1 周末评审后冻结） |
| 适用工程 | logisim-lite（C++20 / wxWidgets 3.2 / wxFormBuilder 3.10 / CMake / vcpkg） |
| 技术栈 | MSVC 2022 + vcpkg，Debug/Release x64 |
| 维护人 | D（模型与集成） |
| 评审人 | A、B、C |
| 最后更新 | 2026-11-05 |
| 状态 | 草案 —— 第 1 周末冻结为 v1.0，之后按第 13 章流程变更 |

---

## 0. 阅读指南

| 角色 | 必读章节 | 关注点 |
| --- | --- | --- |
| A 编辑器 | 2、4、5、6.3、7、8、10、12 | 怎么拿到元件图元、怎么读仿真结果、怎么发命令 |
| B 仿真核心 | 2、4、5、6、7、10、12 | 元件求值契约、状态外置、网表与事件接口 |
| C 元件库 | 2、4、6、7.2、10 | 注册一个元件需要实现哪些虚函数 |
| D 模型与集成 | 全部 | 模型、JSON、构建、版本 |
| 全员 | 1、2、3、11、13、14 | 边界裁定、依赖规则、错误约定、变更流程 |

**文档约定**

- `MUST` 强制，`SHOULD` 建议，`MAY` 可选。
- 文中代码块是**契约**，不是参考实现；字段名与类型冻结后不得随意改。
- 命名空间统一 `ll`；头文件路径统一 `ll/<module>/<file>.h`。
- 标 `[冻结]` 的接口在 v1.0 后只能新增、不能修改签名。
- 标 `[试用]` 的接口允许在对应周次内调整，但必须当天通知全员。

---

## 1. 目标与范围

### 1.1 本期目标（第 9 周可交付）

- 在画布上放置元件、连线、框选、拖动、旋转、删除、撤销/重做。
- 组合电路仿真（AND/OR/NOT/NAND/NOR/XOR、MUX、译码器、加法器、比较器）。
- 时序电路仿真（时钟、D 触发器、寄存器、计数器）。
- JSON 存取、自动保存、异常文件处理、打包安装。
- 真值表/波形查看（第 7 周）。

### 1.2 本期不做（明确排除，避免接口为它们过度设计）

- 兼容 Logisim 的 `.circ` 格式（我们用 JSON）。
- 三态总线与总线分线器（v1 不做，见 §15 待定项；接口预留 width 字段）。
- 多时钟域、门控时钟。
- 电路仿真回退（time travel）、VCD 导出。
- 插件动态加载（DLL）。

### 1.3 设计原则

1. **依赖单向**：`editor → sim → component → model`，`model` 零依赖。反向 include 一律视为缺陷。
2. **model 是唯一数据源**：元件、导线、电路只存在于 `Document` 中，仿真状态、渲染缓存都是派生数据。
3. **行为与绘制分离**：`component` 只产出纯数据 `DrawList`，不碰 `wxGraphicsContext`；`editor` 负责把数据画成像素。
4. **状态外置**：元件求值是纯函数，时序状态存放在 `ComponentState`，由仿真器持有。
5. **一切编辑走 Command**：任何绕过 `UndoStack` 的模型修改都算 bug。

---

## 2. 模块边界与依赖

### 2.1 分层

```
        ┌──────────────────────────────────────┐
        │  app / editor            (A)         │  wxWidgets、wxFormBuilder
        └───────────────┬──────────────────────┘
                        │
        ┌───────────────┴───────────────┐
        ▼                               ▼
┌──────────────────┐          ┌──────────────────┐
│  sim      (B)    │          │ component  (C)   │
└────────┬─────────┘          └────────┬─────────┘
         │                             │
         └──────────────┬──────────────┘
                        ▼
              ┌──────────────────┐
              │  model    (D)    │   ← 只依赖标准库 + nlohmann/json（仅 .cpp 内）
              └──────────────────┘
```

### 2.2 CMake 目标

| 目标 | 目录 | 负责人 | 依赖 | 允许 include |
| --- | --- | --- | --- | --- |
| `ll_model` | `model/` | D | 无（JSON 仅在 `.cpp` 私有链接） | `<cstdint>`、`<string>`、`<vector>`、`<map>`、`<variant>`、`<span>` |
| `ll_component` | `component/` | C | `ll_model` | 同上 |
| `ll_sim` | `sim/` | B | `ll_component`、`ll_model` | 同上 |
| `ll_editor` | `editor/` | A | `ll_sim`、`ll_component`、`ll_model` | `wx/*` |
| `ll_app` | `app/` | A | `ll_editor` | `wx/*` |
| `tests_model` / `tests_component` / `tests_sim` | `tests/` | 各自 | 对应库 + Catch2 | 禁止 `wx/*` |

**硬规则**：`wx/` 头文件只允许出现在 `editor/`、`app/`、`tests_editor/`。CI 里加一条 `grep -r "wx/" model component sim`，命中即失败。

### 2.3 公共头文件目录

```
model/include/ll/model/*.h
component/include/ll/component/*.h
sim/include/ll/sim/*.h
editor/include/ll/editor/*.h      # 仅 editor 内部使用
```

每个公共头文件顶部必须有一行归属注释：

```cpp
// owner: C   stability: frozen   since: v1.0
```

---

## 3. 分工重叠的裁决（重要）

你们的分工表里有 8 处职责交叠，必须在第 1 周前定死，否则第 4 周会互相等。以下为裁定结果，**写进文档即视为团队决议**。

| # | 交叠点 | 裁定 | 归属 |
| --- | --- | --- | --- |
| 1 | C 说"定义元件、引脚数据结构"，D 说"公共模型" | **静态声明**（`PinDef`/`ComponentDef`/`AttrDef`/`Shape`）放 `component/`，**实例数据**（`ComponentInstance`/`Wire`/`Circuit`）放 `model/` | C 前者，D 后者 |
| 2 | B 说"定义逻辑值"，D 说"公共模型" | `LogicValue` 放 `model/logic_value.h`，因为四层都要用；B 负责语义定义、边界用例与测试 | 头文件 D，语义 B |
| 3 | C 的"元件"与 A 的画布 | C 只产出 `DrawList`（本地坐标纯数据），A 负责旋转/翻转/缩放并画到 `wxGraphicsContext` | C 出数据，A 出像素 |
| 4 | A 的"属性面板"与 C 的元件属性 | C 声明 `AttrDef`（键、类型、默认值、取值范围），A 据此**自动生成**控件，禁止按元件硬编码表单 | C 声明，A 渲染，D 存储 |
| 5 | B 的"网表编译"与 A 的"连接点/高亮" | 连通性只由 `ll::sim::BuildNetlist()` 计算，A 不得自己写并查集；A 调它做高亮，结果按 `Document::Revision()` 缓存 | B |
| 6 | C 的"子电路封装" | 层次结构存 `model`；A 提供"从选区创建子电路"命令；B 在网表阶段展开；C 负责把子电路注册成运行时元件类型 | 四方各一段，见 §7.5 |
| 7 | A 的"撤销重做" | `UndoStack` 归 A；但 `model` **必须**提供保留原 ID 的 `Restore*` 接口，否则重做后引用全断 | A 栈，D 恢复接口 |
| 8 | B 的"时钟"与 C 的"时钟元件" | 时钟节拍由 B 的调度器产生，C 只声明"我是时钟源"和符号；A 只发 Run/Pause/Step | B 驱动，C 声明 |
| 9 | A 的"文件菜单"与 D 的"JSON" | 格式、读写、迁移归 D；文件对话框、最近文件、未保存提示归 A | D 格式，A 交互 |
| 10 | B 的"波形数据"与 A 的"波形界面" | B 记录采样，A 只负责画 | B 数据，A 绘制 |

---

## 4. 公共基础类型（`model/types.h`，D 维护）

```cpp
// owner: D  stability: frozen
#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace ll {

using ComponentId = std::uint32_t;
using WireId      = std::uint32_t;
using CircuitId   = std::uint32_t;
inline constexpr ComponentId kInvalidComponent = 0;

// 坐标：半格为单位，1 个网格 = 2 单位。整数运算，禁止浮点进模型。
struct GridPoint { int x = 0; int y = 0; };
struct GridRect  { GridPoint tl, br; };

constexpr GridPoint operator+(GridPoint a, GridPoint b) { return {a.x + b.x, a.y + b.y}; }
constexpr bool operator==(GridPoint a, GridPoint b) { return a.x == b.x && a.y == b.y; }

enum class PinDir : std::uint8_t { Input, Output, InOut };
enum class LogicState : std::uint8_t { Zero, One, Unknown, HighZ };

} // namespace ll
```

### 4.1 `LogicValue`（`model/logic_value.h`）

64 位以内、按位三态。用三个掩码表示，避免 `std::vector<bool>` 带来的性能与序列化麻烦。

```cpp
// owner: D(结构) / B(语义)  stability: frozen
struct LogicValue {
    std::uint64_t value = 0;   // 逻辑 1 的位
    std::uint64_t xmask = 0;   // 1 = Unknown (冲突/未驱动为逻辑值)
    std::uint64_t zmask = 0;   // 1 = HighZ  (悬空)
    std::uint8_t  width = 1;   // 1..64

    static LogicValue Zero(std::uint8_t w = 1);
    static LogicValue One(std::uint8_t w = 1);
    static LogicValue Unknown(std::uint8_t w = 1);
    static LogicValue HighZ(std::uint8_t w = 1);
    static LogicValue FromUInt(std::uint64_t v, std::uint8_t w);

    bool AllKnown() const;                 // 无 X 无 Z
    LogicState Bit(int i) const;           // 0..width-1
    LogicValue Bits(int lo, int hi) const; // 取位段
    LogicValue Zext(std::uint8_t w) const;
    LogicValue Trunc(std::uint8_t w) const;
    bool operator==(const LogicValue&) const = default;
};
```

**B 负责的语义**（写进 `tests_sim/logic_value_test.cpp`）：

| 场景 | 结果 |
| --- | --- |
| 单 bit 驱动多 bit 网络 | 广播到所有位 |
| 多 bit 驱动单 bit 网络 | 取 bit0 |
| 两个驱动值相同 | 正常值 |
| 两个驱动值不同 | 对应位 `Unknown` |
| 无驱动 | `HighZ` |
| `Unknown` 参与与/或 | 按三值逻辑：`1 & X = X`，`0 & X = 0`，`1 \| X = 1`，`0 \| X = X` |

### 4.2 属性值

```cpp
// owner: D  stability: frozen
using AttrValue = std::variant<std::int64_t, double, bool, std::string>;
using AttrMap   = std::map<std::string, AttrValue>;   // 有序，保证 JSON 输出稳定
```

> 用 `std::map` 而不是 `unordered_map`：序列化结果可直接进 git 做 diff，联调时非常省事。

### 4.3 返回状态

```cpp
// owner: D  stability: frozen
enum class StatusCode {
    Ok, InvalidArgument, NotFound, DuplicateName, CycleDetected,
    ParseError, VersionTooNew, IoError, UnsupportedType
};

struct Status {
    StatusCode code = StatusCode::Ok;
    std::string message;                 // 面向用户的中文提示
    explicit operator bool() const { return code == StatusCode::Ok; }
    static Status Ok();
    static Status Fail(StatusCode, std::string msg);
};
```

**约定**：模型层违反不变式 → `assert`（编程错误）；用户可触发的失败 → 返回 `Status`，不抛异常。仿真错误不返回 `Status`，走 `Engine::Errors()`（见 §7.6）。

---

## 5. 模型层接口（`model/`，D）

### 5.1 数据结构

```cpp
// owner: D  stability: frozen
struct ComponentInstance {
    ComponentId id = kInvalidComponent;
    std::string type;          // 对应 ComponentDef::Type()
    GridPoint   position;      // 元件原点
    int         rotation = 0;  // 0/90/180/270，逆时针
    bool        mirrored = false;
    AttrMap     attrs;
    std::string label;         // 用户可见标签，可为空
};

struct PinRef { ComponentId component = kInvalidComponent; std::uint16_t pin = 0; };

struct WireEndpoint {
    bool     attached = false; // true 表示端点吸附在引脚上
    PinRef   pin;              // attached 时有效
    GridPoint point;           // 未吸附时的自由坐标
};

struct Wire {
    WireId  id = 0;
    std::vector<GridPoint> points;  // 折线顶点，含两端，至少 2 个
    WireEndpoint from, to;
};

struct Port {                       // 子电路对外引脚
    std::string name;
    PinDir      dir = PinDir::Input;
    std::uint8_t width = 1;
    GridPoint   position;
};
```

```cpp
struct ComponentSpec {              // 新增元件时由编辑器填写
    std::string type;
    GridPoint   position;
    int         rotation = 0;
    bool        mirrored = false;
    AttrMap     attrs;
    std::string label;
};

struct WireSpec {
    std::vector<GridPoint> points;
    WireEndpoint from, to;
};
```

### 5.2 `Circuit`

```cpp
// owner: D  stability: frozen
class Circuit {
public:
    CircuitId Id() const;
    std::string_view Name() const;

    const std::vector<ComponentInstance>& Components() const;
    const std::vector<Wire>& Wires() const;
    const std::vector<Port>& Ports() const;

    const ComponentInstance* FindComponent(ComponentId) const;
    const Wire*              FindWire(WireId) const;

    // ---- 正常编辑：分配新 ID，返回新 ID ----
    ComponentId AddComponent(const ComponentSpec&);
    WireId      AddWire(const WireSpec&);
    bool        RemoveComponent(ComponentId);   // 级联删除吸附其上的导线端点
    bool        RemoveWire(WireId);
    bool        SetAttr(ComponentId, std::string key, AttrValue);
    bool        SetPosition(ComponentId, GridPoint);
    bool        SetTransform(ComponentId, int rotation, bool mirrored);

    // ---- 撤销/重做专用：保留原 ID ----
    void RestoreComponent(ComponentInstance);   // ID 已存在则 ASSERT 失败
    void RestoreWire(Wire);
    void RestorePort(Port);

    // 该电路内所有 ID 的最大值，序列化用
    ComponentId MaxComponentId() const;
    WireId      MaxWireId() const;
};
```

**不变式（D 必须用 assert 守住）**

1. 同一 `Circuit` 内 `ComponentId`、`WireId` 唯一；删除后**不复用**。
2. `rotation ∈ {0,90,180,270}`；`mirrored` 在旋转前后语义等价于"先镜像再旋转"。
3. `Wire::points` 至少 2 点，相邻点共线时自动合并，不得有重复点。
4. `WireEndpoint::attached == true` 时，`pin.component` 必须存在且 `pin.pin` 在该元件引脚数范围内。
5. 子电路不得直接或间接包含自身（`Document::Validate()` 检查）。

### 5.3 `Document`

```cpp
// owner: D  stability: frozen
class Document {
public:
    static Document CreateEmpty(std::string main_circuit = "main");

    Circuit&       Main();
    const Circuit& Main() const;

    Circuit*       FindCircuit(std::string_view name);
    Circuit&       CreateCircuit(std::string_view name);   // 重名返回 Status 失败版
    bool           RemoveCircuit(std::string_view name);   // main 不可删

    const std::vector<std::unique_ptr<Circuit>>& Circuits() const;

    std::uint64_t Revision() const;   // 任何结构/属性变更 +1
    bool          Dirty() const;
    void          MarkSaved();

    Status Validate() const;          // 引用完整性 + 子电路无环
};
```

**`Revision` 的用法**：A 用它判断是否需要重建网表、C 用它做符号缓存失效。凡是会改变连通性或元件参数的操作都必须 `++revision`，包括撤销。

### 5.4 模型不负责的事（防止职责膨胀）

- **不校验 `type` 是否存在**：类型合法性由编辑器和 `ComponentRegistry` 决定，model 只存字符串。
- **不算引脚坐标**：引脚位置依赖 `ComponentDef::Pins()`，由 `component` 层提供 `PinWorldPos()`（§6.4）。
- **不算连通性**：归 B 的 `BuildNetlist()`。
- **不加载 JSON**：序列化在 `model/src/json_*.cpp` 内实现，但接口以自由函数暴露（§9）。

---

## 6. 元件层接口（`component/`，C）

### 6.1 引脚与属性声明

```cpp
// owner: C  stability: frozen
struct PinDef {
    std::string name;        // "in0"、"out"、"clk"
    PinDir      dir = PinDir::Input;
    std::uint8_t width = 1;  // 1..64
    GridPoint   offset;      // 相对元件原点，未旋转坐标系
};

enum class AttrType : std::uint8_t { Int, Bool, String, Enum, Color };

struct EnumItem { std::string value; std::string label; };

struct AttrDef {
    std::string key;                       // "inputs"
    std::string label;                     // "输入引脚数"
    AttrType    type = AttrType::Int;
    AttrValue   default_value;
    std::int64_t min = 0, max = 0;         // Int 用
    std::vector<EnumItem> options;         // Enum 用
};
```

> A 的属性面板**只读 `AttrDef`**：`Int → wxSpinCtrl`，`Bool → wxCheckBox`，`Enum → wxChoice`，`String → wxTextCtrl`，`Color → wxColourPickerCtrl`。新增元件不需要改面板代码。

### 6.2 求值接口

```cpp
// owner: C  stability: frozen
struct EvalContext {
    // 与 Pins() 同序同长。输出引脚给的是当前网表值（供三态/反馈元件读取）。
    std::span<const LogicValue> pins;
    const AttrMap& attrs;
    std::uint64_t  tick = 0;
    bool           clock_level = false;   // 全局时钟当前电平
    bool           clock_rising = false;  // 本 tick 是否上升沿
};

struct EvalResult {
    // 与 Pins() 同序同长；只需写 Output/InOut 位置，其余忽略。
    std::span<LogicValue> pins;
};

class ComponentState {                     // 时序元件的私有状态基类
public:
    virtual ~ComponentState() = default;
};

class ComponentDef {
public:
    virtual ~ComponentDef() = default;

    // ---- 元数据 ----
    virtual std::string_view Type() const = 0;        // "and"，全局唯一
    virtual std::string_view Category() const = 0;    // "Gates"、"Memory"、"I/O"
    virtual std::string_view DisplayName() const = 0; // "与门"
    virtual std::vector<AttrDef> Attrs() const { return {}; }
    virtual std::vector<PinDef>  Pins(const AttrMap&) const = 0;  // 引脚数可随属性变化

    // ---- 行为 ----
    virtual void Eval(const EvalContext&, ComponentState*, EvalResult&) const = 0;

    // ---- 时序（默认组合元件不实现）----
    virtual bool IsSequential() const { return false; }
    virtual std::unique_ptr<ComponentState> CreateState() const { return nullptr; }
    virtual void Reset(ComponentState*) const {}
    virtual void CommitEdge(const EvalContext&, ComponentState*) const {}  // 上升沿提交

    // ---- 特殊类别 ----
    virtual bool IsClockSource() const { return false; }
    virtual bool IsSubcircuit()  const { return false; }

    // ---- 外观 ----
    virtual DrawList BaseSymbol(const AttrMap&) const = 0;
    virtual bool     HasLiveSymbol() const { return false; }
    virtual void     BuildLiveSymbol(const EvalContext&, DrawList&) const {}
};
```

### 6.3 `DrawList`：纯数据图元

C 产出、A 消费。**坐标一律是元件本地坐标系（未旋转、未缩放），原点在元件原点，单位与网格一致。**

```cpp
// owner: C  stability: frozen
enum class ColorRole : std::uint8_t { Ink, Fill, Background, Accent, Error, Pin };

struct Shape {
    enum class Kind : std::uint8_t { Line, Rect, Polyline, Circle, Arc, Text };
    Kind kind = Kind::Line;

    std::vector<GridPoint> points;  // Line/Polyline
    GridRect rect;                  // Rect/Circle/Arc 的外接框
    int      start_deg = 0, sweep_deg = 0;   // Arc
    std::string text;               // Text
    int      font_size = 12;        // Text，单位=网格的 1/10

    ColorRole stroke = ColorRole::Ink;
    ColorRole fill   = ColorRole::Fill;
    int       stroke_width = 4;     // 单位=网格的 1/10，即 4 = 0.4 格
    bool      filled = false;
};

struct DrawList { std::vector<Shape> shapes; };
```

**约定**

- `BaseSymbol()` 不含引脚圆点、选中框、标签，这些由 A 统一绘制（保证风格一致、可命中测试）。
- 动态外观（LED 亮灭、七段显示、探针数值）用 `BuildLiveSymbol()` 在 `BaseSymbol` 结果上追加/覆盖。
- A 负责 `rotation`/`mirrored` 变换与 `grid → 像素` 缩放，C 不得假设任何像素尺寸。

### 6.4 注册表与几何辅助

```cpp
// owner: C  stability: frozen
class ComponentRegistry {
public:
    void Register(std::unique_ptr<ComponentDef>);
    const ComponentDef* Find(std::string_view type) const;   // 找不到返回 nullptr
    std::vector<const ComponentDef*> ByCategory(std::string_view) const;  // 按注册顺序
    std::vector<std::string> Categories() const;
};

// 进程内唯一、只读。测试可自建 registry，不要依赖单例。
const ComponentRegistry& BuiltinComponents();

// 几何辅助：把引脚偏移变换到世界坐标（编辑器与仿真共用，禁止各写一份）
GridPoint PinWorldPos(const ComponentInstance&, const ComponentDef&, std::uint16_t pin_index);
GridRect  ComponentBounds(const ComponentInstance&, const ComponentDef&, const AttrMap&);
```

**注册方式**：`BuiltinComponents()` 内部按类别注册，C 新增元件只需在 `component/src/builtin.cpp` 加一行 `r.Register(std::make_unique<AndGateDef>());`，无需改 A/B/D。

### 6.5 `Eval` 的实现契约（C 必须遵守，B 负责验收）

1. **纯函数**：给定相同 `pins`/`attrs`/`state`/`tick`，输出必须完全一致；不得读全局变量、不得做 I/O、不得缓存跨实例数据。
2. **不修改 `state`**：`Eval` 只读 `ComponentState`；需要改状态时用 `CommitEdge`。
3. **只写输出口**：`EvalResult::pins` 中 Input 位置写了也会被忽略，但为可读性请勿写。
4. **宽度规则**：输出位宽必须等于 `Pins()` 声明的宽度；内部计算可用 `std::uint64_t`，超出位宽截断。
5. **不做连通性判断**：多驱动、悬空由 B 统一处理，元件只管"我驱动什么值"。
6. **每个元件必须有一组真值表测试**，放在 `tests_component/`，由 B 的 CI 一起跑。

### 6.6 时序元件的两段式协议

```
tick N:
  ① B 计算 settle（Eval，读旧 state）
  ② 若上升沿：B 对所有 IsSequential() 元件调 CommitEdge（写 state）
  ③ 再 settle 一次，让 Q 的新值传播出去
```

D 触发器示例：

```cpp
void DffDef::Eval(const EvalContext& ctx, ComponentState* st, EvalResult& out) const {
    auto* s = static_cast<DffState*>(st);
    out.pins[1] = s->q;              // Q  = 上一次提交的值
    out.pins[2] = ~s->q;             // Q' （按位取反，保持宽度）
}

void DffDef::CommitEdge(const EvalContext& ctx, ComponentState* st) const {
    auto* s = static_cast<DffState*>(st);
    s->q = ctx.pins[0];              // 上升沿采样 D
}
```

### 6.7 子电路元件的特殊约定

`IsSubcircuit() == true` 的元件**不会被调用 `Eval`**。B 的网表构建器遇到它时直接展开内部电路（引脚按 `Ports()` 映射）。C 提供：

```cpp
class SubcircuitDef : public ComponentDef {
public:
    explicit SubcircuitDef(CircuitId target, std::string type_name);
    CircuitId Target() const;
    // Pins() 由目标电路的 Ports() 生成；BaseSymbol() 画一个带名字的方框
    // Eval() 实现为 assert(false)，防止误调用
};
```

---

## 7. 仿真层接口（`sim/`，B）

### 7.1 网表

```cpp
// owner: B  stability: frozen
struct NetId { std::uint32_t value = 0; };

class Netlist {
public:
    std::size_t NetCount() const;
    const std::vector<PinRef>& PinsOf(NetId) const;
    NetId NetOf(PinRef) const;                       // 每个引脚恰好属于一个网
    const std::vector<NetId>& Nets() const;
    std::uint64_t Fingerprint() const;               // 结构哈希，用于缓存命中
};

// 唯一允许计算连通性的入口；编辑器高亮也用它
Netlist BuildNetlist(const Document&);
```

**连通规则**（B 实现，A/C 不得重复实现）：

- 导线端点 `attached` → 直接与 `PinRef` 同网。
- 两条导线端点坐标相同 → 同网。
- 导线端点落在另一条导线线段中间 → 同网（T 型连接）。
- 交叉但无端点 → **不**同网（和 Logisim 一致，需要连接点时用户加点）。
- 单引脚网络合法（悬空）。

### 7.2 引擎

```cpp
// owner: B  stability: frozen
namespace ll::sim {

enum class RunMode : std::uint8_t { Stopped, Running, Paused };

struct PinValueChange { PinRef pin; LogicValue value; };

struct SimFrame {
    std::uint64_t tick = 0;
    std::span<const PinValueChange> changes;   // 本 tick 发生变化的引脚
    std::span<const NetlistError>   errors;
    bool converged = true;
};

using Listener = std::function<void(const SimFrame&)>;

class Engine {
public:
    explicit Engine(const ComponentRegistry& registry);
    ~Engine();

    // 装载：编译网表 + 分配状态。失败时保留旧网表不变。
    Status Load(const Document&);

    void Reset();                       // 状态清零，输入置 Zero，tick=0
    void Tick();                        // 推进半个时钟周期
    void Advance(std::uint64_t ticks);

    void SetInput(PinRef, LogicValue);  // 仅对 Input 引脚有效
    LogicValue Value(PinRef) const;     // 该引脚当前看到的网表值
    LogicValue NetValue(NetId) const;

    std::uint64_t TickCount() const;
    bool ClockLevel() const;

    const std::vector<NetlistError>& Errors() const;
    const Netlist& NetlistRef() const;

    void SetListener(Listener);         // 每 tick 末尾回调一次，无变化也会回调
    void SetRecording(bool);            // 波形采样开关
    const Waveform& GetWaveform() const;
};
```

**时钟约定（v1，全局单时钟）**

- 每 2 个 tick 为一个时钟周期；`clock_level = ((tick + 1) / 2) % 2 == 1`。
- 上升沿发生在 `tick` 为奇数时（即 tick 1,3,5,…）。**此定义由 `tests_sim/clock_test.cpp` 锁定**，任何人改都要改测试并在群里公告。
- `IsClockSource()` 的元件输出口由引擎直接写 `clock_level`，`Eval` 不参与。

**`Tick()` 内部顺序（规范）**

```
1. tick_ += 1，更新 clock_level，判断 rising
2. Settle(rising)          // 迭代求值至稳定
3. if (rising) 对所有 IsSequential 元件 CommitEdge()
4. if (rising) Settle(false)   // 传播新状态
5. 采样波形、构造 SimFrame、调 Listener
```

`Settle` 规则：

- 按 `ComponentId` 升序求值，保证确定性。
- 每次把输出写入网络；若任何网络值变化，再迭代一轮。
- 最大迭代 200 轮；超限判定为振荡，把参与振荡的网标为 `Unknown`，写 `Oscillation` 错误，`converged = false`，本 tick 结束（不阻塞 UI）。

### 7.3 诊断

```cpp
// owner: B  stability: frozen
struct NetlistError {
    enum class Kind : std::uint8_t {
        MultipleDrivers,   // 多个输出引脚驱动同一网且值冲突
        FloatingInput,     // 输入引脚所在网络无驱动
        Oscillation,       // 组合环无法收敛
        WidthMismatch,     // 连接位宽不一致
        FloatingNet        // 网络无任何驱动（仅提示）
    };
    Kind kind;
    std::vector<NetId> nets;
    std::string message;   // 已本地化，可直接显示
};
```

错误**不抛异常、不中断仿真**，只累积到 `Errors()`，由 A 决定怎么提示。同一 `(kind, nets)` 只报一次，避免每 tick 刷屏。

### 7.4 层次展开

`BuildNetlist` 递归展开 `IsSubcircuit()` 元件：

- 子电路内元件的 `ComponentId` 在扁平网表中加前缀重编号，对外不可见。
- 子电路 `Port` 与实例引脚一一映射，直接合并到外层网络。
- 展开深度上限 64，超限报 `CycleDetected`（D 的 `Validate()` 应该更早拦住）。
- 展开后的网表**不含**子电路实例本身。

### 7.5 波形

```cpp
// owner: B  stability: 试用
struct WaveSample {
    std::uint64_t tick;
    NetId net;
    LogicValue value;
};

class Waveform {
public:
    std::size_t SampleCount() const;
    const std::vector<WaveSample>& Samples() const;
    const std::vector<NetId>& WatchedNets() const;
    void SetWatched(std::vector<NetId>);   // 为空表示全记录
    void Clear();
};
```

采样点只在值变化时记录（边沿存储），A 绘制时做阶梯插值。上限 50 万条，超出丢最旧的 25%。

---

## 8. 编辑器层（`editor/`，A）

### 8.1 命令

```cpp
// owner: A  stability: 试用（接口简单，随 UI 演进）
class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view Name() const = 0;      // "添加与门"，用于菜单显示
    virtual void Do(Document&) = 0;
    virtual void Undo(Document&) = 0;
    virtual bool MergeWith(const Command&) { return false; }  // 连续拖动合并
};

class UndoStack {
public:
    void Push(Document&, std::unique_ptr<Command>);  // Push 内部执行 Do
    bool Undo(Document&);
    bool Redo(Document&);
    bool CanUndo() const;
    bool CanRedo() const;
    std::string_view UndoName() const;
    std::string_view RedoName() const;
    void Clear();
    std::size_t Depth() const;                       // 上限 200，超出丢最旧
};
```

**要求**：所有 `Do` 内只调用 `Circuit` 的公开接口；删除类命令必须先在 `Do` 前把完整 `ComponentInstance`/`Wire` 拷进命令对象，`Undo` 时用 `Restore*` 原样放回（ID 不变）。

### 8.2 工具与命中测试

```cpp
struct HitResult {
    enum class Kind : std::uint8_t { None, Component, Pin, Wire, WirePoint, Port };
    Kind kind = Kind::None;
    ComponentId component = kInvalidComponent;
    std::uint16_t pin = 0;
    WireId wire = 0;
    int point_index = -1;
};

class ToolController {
public:
    virtual ~ToolController() = default;
    virtual void OnMouseDown(const CanvasEvent&) = 0;
    virtual void OnMouseMove(const CanvasEvent&) = 0;
    virtual void OnMouseUp(const CanvasEvent&)   = 0;
    virtual void OnKeyDown(const wxKeyEvent&)    = 0;
    virtual void OnPaintOverlay(wxDC&, const Viewport&) {}   // 连线预览等
    virtual void Cancel() = 0;
    virtual std::string_view Hint() const = 0;   // 状态栏提示
};

struct CanvasEvent {
    GridPoint world;      // 已吸附网格
    wxPoint   screen;
    int       zoom_percent;
    bool      shift, ctrl, alt;
    wxMouseEvent raw;
};
```

### 8.3 A 对其他三方的义务

| 义务 | 说明 |
| --- | --- |
| 编辑前暂停仿真 | `engine.Stop()` → 改模型 → 若 `Revision` 变化则 `engine.Load(doc)` |
| 不自己算连通性 | 高亮/选中网络一律调 `BuildNetlist`，按 `Revision` 缓存 |
| 不硬编码元件外观 | 全部走 `BaseSymbol()`/`BuildLiveSymbol()` |
| 不硬编码属性表单 | 全部由 `AttrDef` 生成 |
| 撤销后重建网表 | 撤销同样 `++Revision`，同样触发 `Load` |
| 不阻塞主线程 | 单次 `Settle` 超过 16ms 须分帧或降 tick 频率（见 §12.3） |

### 8.4 渲染管线（A 内部，写清便于 C 配合）

```
OnPaint:
  BeginBufferedPaint
  1. 画网格（按 zoom 决定细/粗线）
  2. 取可见矩形（world 坐标），剔除不可见元件与导线
  3. 画导线（先画，压在元件下）+ 连接点圆点
  4. 对每个可见元件：
       dl = def->BaseSymbol(attrs)
       if (def->HasLiveSymbol()) def->BuildLiveSymbol(ctx, dl)
       ApplyTransform(dl, instance.position, rotation, mirrored, zoom)
       Execute(dl, gc)          // Shape → wxGraphicsContext 调用
     画引脚圆点（用 PinWorldPos）
  5. 画选中框、悬停高亮、工具预览（OnPaintOverlay）
  6. 画标签与实时电平（探针/七段）
  EndBufferedPaint
```

`Execute(Shape)` 里 `ColorRole → wxColour` 的映射集中在 `editor/theme.cpp`，换配色只改一处。

---

## 9. 序列化（`model/`，D）

### 9.1 格式

JSON，UTF-8 无 BOM，`\n` 换行，2 空格缩进，键排序（`std::map` 天然有序）。

```json
{
  "format": "logisim-lite",
  "version": 1,
  "main": "main",
  "circuits": [
    {
      "id": 1,
      "name": "main",
      "ports": [],
      "components": [
        { "id": 10, "type": "input", "x": 80, "y": 100, "rot": 0, "flip": false,
          "label": "A", "attrs": {} },
        { "id": 11, "type": "input", "x": 80, "y": 140, "rot": 0, "flip": false,
          "label": "B", "attrs": {} },
        { "id": 12, "type": "and", "x": 160, "y": 120, "rot": 0, "flip": false,
          "label": "", "attrs": { "inputs": 2 } },
        { "id": 13, "type": "led", "x": 260, "y": 120, "rot": 0, "flip": false,
          "label": "", "attrs": { "color": "red" } }
      ],
      "wires": [
        { "id": 100, "points": [[100,100],[140,100],[140,118],[160,118]],
          "from": { "component": 10, "pin": 0 },
          "to":   { "component": 12, "pin": 0 } },
        { "id": 101, "points": [[100,140],[140,140],[140,122],[160,122]],
          "from": { "component": 11, "pin": 0 },
          "to":   { "component": 12, "pin": 1 } },
        { "id": 102, "points": [[196,120],[260,120]],
          "from": { "component": 12, "pin": 2 },
          "to":   { "component": 13, "pin": 0 } }
      ]
    }
  ]
}
```

**规则**

1. `version` 是整数，当前为 `1`。读到更高版本 → 拒绝加载，提示"文件由更新版本创建"。
2. 未知字段**忽略**（前向兼容）；缺失字段用默认值（后向兼容）。
3. `points` 是完整折线（含贴引脚的那一点），`from`/`to` 只做**绑定提示**。加载时若绑定与坐标不一致，以绑定为准重新吸附，并记一条 warning。
4. 不保存仿真状态、不保存窗口布局、不保存最近文件。
5. 保存必须**原子**：写 `xxx.json.tmp` → `fsync` → `rename` 覆盖。中途崩溃不能损坏原文件。
6. 自动保存到 `%APPDATA%/logisim-lite/autosave.json`，间隔 2 分钟，正常保存成功后删除。

### 9.2 接口

```cpp
// owner: D  stability: frozen
struct LoadOptions {
    bool strict = false;        // true 时 warning 也视为失败
    bool migrate = true;
};
struct LoadResult {
    Status status;
    Document document;
    std::vector<std::string> warnings;   // 悬空导线、未知属性等
};

LoadResult LoadDocument(const std::filesystem::path&, const LoadOptions& = {});
Status     SaveDocument(const Document&, const std::filesystem::path&);

// 内存互转，测试与剪贴板用
LoadResult LoadDocumentFromString(std::string_view json);
std::string SaveDocumentToString(const Document&);
```

**迁移**：`version < CURRENT` 时依次调用 `Migrate_1_to_2`、`Migrate_2_to_3`……每加一版必须配一个旧文件 fixture 和一个迁移测试。

**剪贴板**：复用 `SaveDocumentToString` 序列化选中的子图（一个临时 `Document`），粘贴时重新分配 ID 并保持相对坐标。

---

## 10. 典型调用时序

### 10.1 放置元件

```
用户点 palette「与门」
  A: PalettePanel 记录待放置类型
  A: ToolController 切到 PlacingTool，状态栏提示"点击画布放置"
用户点画布
  A: PlacingTool 命中 GridPoint
  A: auto cmd = make_unique<AddComponentCommand>(type, pos, reg.Find(type)->Attrs() 默认值)
  A: undo.Push(doc, move(cmd))       → Command::Do → Circuit::AddComponent → Revision++
  A: engine.Stop(); engine.Load(doc) → B: 重建网表、分配 state
  A: canvas.Refresh()
```

### 10.2 连线

```
按下在某引脚上
  A: HitTest → Pin；WireTool 记录起点 PinRef
移动中
  A: 计算正交折线预览（只用几何，不碰模型）
松开在另一引脚/导线
  A: 若端点吸附成功 → AddWireCommand(points, from, to)
  A: Circuit::AddWire → Revision++
  A: engine.Load(doc)
```

### 10.3 运行 / 单步

```
A: wxTimer 每 100ms（默认）触发
A: if (mode == Running) engine.Advance(n_ticks_per_frame)
B: Tick() × n → 期间累积 changes
B: 最后一个 tick 末尾调 Listener(frame)
A: 把 frame.changes 存进待重绘集合 → canvas.Refresh(false)（只重绘脏区）
注：Listener 在 UI 线程被调用（v1 单线程模型）
```

### 10.4 保存

```
A: File → Save
D: SaveDocument(doc, path) → 序列化 → 原子写
A: doc.MarkSaved(); 更新标题栏（去 *）
失败 → D 返回 Status → A 弹 wxMessageBox
```

### 10.5 撤销

```
A: undo.Undo(doc) → Command::Undo → Circuit::Restore*（ID 不变）→ Revision++
A: engine.Stop(); engine.Load(doc)
A: 菜单 Enable(CanUndo/CanRedo) 由 UpdateUI 统一刷新
```

---

## 11. 错误处理约定

| 层 | 编程错误 | 用户可触发错误 |
| --- | --- | --- |
| model | `assert`，Debug 崩、Release 记录日志 | 返回 `Status` |
| component | `assert`（如 `Eval` 访问越界） | 不产生，交给 sim |
| sim | `assert`（网表未加载就 Tick） | `NetlistError` 累积，不中断 |
| editor | `assert` | `wxMessageBox` / 状态栏 / 日志面板 |

**日志**：`ll::Log(Level, format...)`，Debug 输出到 `OutputDebugString` + 文件，Release 只写文件。日志文件滚动，单文件 5MB，保留 3 个。

**禁止**：在库层调用 `wxMessageBox`；在库层 `std::cout`；用异常做控制流。

---

## 12. 线程与生命周期

### 12.1 所有权

| 对象 | 拥有者 | 生命周期 | 备注 |
| --- | --- | --- | --- |
| `Document` | A 的 `EditorApp` | 应用级 | `std::unique_ptr`，打开新文件时整体替换 |
| `Engine` | A 的 `EditorFrame` | 文档级 | `Load()` 复用实例，不重建 |
| `ComponentState` | B 的 `Engine` | 网表级 | `Load` 时 `CreateState`，`Reset` 时 `Reset` |
| `Netlist` | B 的 `Engine` | 网表级 | 按 `Revision` 重建 |
| `ComponentRegistry` | 进程单例 | 全局 | 只读，测试可替换 |
| `DrawList` | A 的渲染临时对象 | 单帧 | 复用一个 `DrawList` 成员，避免每帧分配 |

### 12.2 线程模型（v1 明确单线程）

- **所有仿真在 UI 线程执行**，通过 `wxTimer` 驱动。理由：v1 电路规模小，单线程可以彻底避免数据竞争。
- 若第 8 周性能测试不达标，再引入工作线程，届时的约束：
  - 仿真期间 `Document` 只读；A 必须 `Stop()` 后才能改模型。
  - 工作线程只调 `engine.Tick()`，结果通过 `wxThreadEvent` 抛回主线程。
  - `Engine` 不提供内部锁，跨线程调用是未定义行为。
- **禁止**在工作线程里碰任何 `wxWindow`。

### 12.3 卡顿兜底

单帧预算 16ms。若 `Advance` 超预算：

1. 降低每帧 tick 数（最少 1）。
2. 仍超 → 显示"仿真较慢"提示，允许用户调低时钟频率。
3. 超过 500ms → 暂停并报 `Oscillation` 或"电路过大"。

---

## 13. 版本与变更流程

### 13.1 冻结计划

| 周次 | 动作 |
| --- | --- |
| 第 1 周末 | 本文件冻结为 v1.0，`model`/`component`/`sim` 头文件落地，全员能编译空实现 |
| 第 2 周 | 端到端跑通 AND + LED，接口第一次真实校验 |
| 第 3 周 | 连线与网表接口稳定，`BuildNetlist` 不再改签名 |
| 第 5 周 | 时序接口（`CommitEdge`、`ComponentState`）冻结 |
| 第 7 周 | 波形、子电路接口冻结 |
| 第 9 周 | 全面冻结，只修 bug |

### 13.2 变更规则

- 改 `[冻结]` 接口：提 PR → 对应 owner approve → 在 `docs/CHANGELOG.md` 记一条 → 群里 @全员。
- 新增虚函数必须给默认实现；禁止删除或改签名（只能新增重载/新函数）。
- 每周一 15 分钟接口对齐会，只过"谁要改什么"，不讨论实现。
- `CODEOWNERS`：

```
/model/      @D
/component/  @C
/sim/        @B
/editor/     @A
CMakeLists.txt @D
```

### 13.3 兼容性承诺

同一大版本内：旧 JSON 必须能打开；旧测试必须继续通过。破坏性变更 → 升 `version` 字段并提供迁移函数。

---

## 14. 测试与联调

### 14.1 各层最小测试集

| 层 | 必测 | 负责人 |
| --- | --- | --- |
| model | ID 唯一/不复用、`Restore*` 保 ID、`Validate` 抓环、JSON round-trip、版本迁移 | D |
| component | 每个元件真值表、`Reset`、宽度规则、`CommitEdge` 时序 | C |
| sim | AND 传播、多驱动、悬空、振荡、DFF 边沿、计数器 0→9、网表展开 | B |
| editor | 冒烟：放置→连线→运行→撤销→存→开（手工清单 + 1 个自动化） | A |

### 14.2 黄金用例

`tests/data/` 下放 `.json` 电路 + 期望结果：

```
and_led.json          → 输入 11 时 LED 亮
dff_toggle.json       → 每两个 tick Q 翻转一次
counter_mod10.json    → 10 个 tick 一轮回
oscillator.json       → 报 Oscillation，不卡死
mux4.json             → 选择端 0..3 的输出表
```

B 的测试直接 `LoadDocument` 这些文件跑仿真，D 的测试直接 `SaveDocument` 后比对文本。

### 14.3 每周联调清单

第 N 周末必须能演示：

1. A 能在 palette 里看到 C 本周新增的所有元件。
2. A 放置后，B 能正确仿真，A 能显示电平。
3. D 能存盘、重开、内容一致。
4. 撤销 10 步后电路与操作前逐字节一致（序列化比对）。

任何一条不满足，下周优先修，不加新功能。

---

## 15. 待定项（第 1 周评审必须拍板）

| # | 问题 | 备选 | 影响 |
| --- | --- | --- | --- |
| 1 | v1 是否做总线/分线器 | 做 / 只留 `width` 字段 | 影响 `Netlist` 与 `LogicValue` 复杂度，建议**不做** |
| 2 | 时钟是全局还是每元件独立频率 | 全局 / 每元件 | 每元件会让 `Tick` 语义复杂一个量级，建议**全局** |
| 3 | 连续拖动是否合并为一条撤销记录 | 合并 / 不合并 | 影响 `Command::MergeWith` 的实现成本 |
| 4 | 子电路是否支持多层嵌套 | 1 层 / 任意层 | 任意层要处理重名与递归，D 的 `Validate` 成本上升 |
| 5 | 波形是否随文件保存 | 保存 / 不保存 | 保存会让 JSON 体积暴涨，建议**不保存** |
| 6 | 探针（Probe）是否 v1 必备 | 做 / 延后 | 影响第 5 周排期 |

---

## 16. 附录

### A. 头文件清单

| 头文件 | 负责人 | 稳定性 | 主要内容 |
| --- | --- | --- | --- |
| `ll/model/types.h` | D | frozen | ID、GridPoint、PinDir、LogicState、Status |
| `ll/model/logic_value.h` | D/B | frozen | `LogicValue` |
| `ll/model/document.h` | D | frozen | `Document` |
| `ll/model/circuit.h` | D | frozen | `Circuit`、`ComponentInstance`、`Wire`、`Port` |
| `ll/model/serialize.h` | D | frozen | `LoadDocument`/`SaveDocument` |
| `ll/component/defs.h` | C | frozen | `PinDef`、`AttrDef`、`Shape`、`DrawList` |
| `ll/component/component_def.h` | C | frozen | `ComponentDef`、`EvalContext`、`EvalResult` |
| `ll/component/registry.h` | C | frozen | `ComponentRegistry`、`BuiltinComponents` |
| `ll/component/geometry.h` | C | frozen | `PinWorldPos`、`ComponentBounds` |
| `ll/sim/netlist.h` | B | frozen | `Netlist`、`BuildNetlist` |
| `ll/sim/engine.h` | B | frozen | `Engine`、`SimFrame`、`RunMode` |
| `ll/sim/error.h` | B | frozen | `NetlistError` |
| `ll/sim/waveform.h` | B | 试用 | `Waveform` |
| `ll/editor/command.h` | A | 试用 | `Command`、`UndoStack` |
| `ll/editor/tool.h` | A | 试用 | `ToolController`、`HitResult` |

### B. 端到端最小示例（口径校验用）

```cpp
#include "ll/model/document.h"
#include "ll/model/serialize.h"
#include "ll/component/registry.h"
#include "ll/sim/engine.h"
#include <cassert>

int main() {
    using namespace ll;

    Document doc = Document::CreateEmpty();
    Circuit&  c   = doc.Main();
    const ComponentRegistry& reg = BuiltinComponents();

    const ComponentId in_a = c.AddComponent({"input", {80, 100},  0, false, {}, "A"});
    const ComponentId in_b = c.AddComponent({"input", {80, 140},  0, false, {}, "B"});
    const ComponentId g    = c.AddComponent({"and",   {160, 120}, 0, false, {}, ""});
    const ComponentId led  = c.AddComponent({"led",   {260, 120}, 0, false, {}, ""});

    c.AddWire({{100,100},{140,100},{140,118},{160,118},
               {true, {in_a, 0}, {}}, {true, {g, 0}, {}}});
    c.AddWire({{100,140},{140,140},{140,122},{160,122},
               {true, {in_b, 0}, {}}, {true, {g, 1}, {}}});
    c.AddWire({{196,120},{260,120},
               {true, {g, 2}, {}}, {true, {led, 0}, {}}});

    assert(doc.Validate());

    sim::Engine engine(reg);
    assert(engine.Load(doc));

    engine.SetInput({in_a, 0}, LogicValue::One());
    engine.SetInput({in_b, 0}, LogicValue::One());
    engine.Tick();

    assert(engine.Value({led, 0}).Bit(0) == LogicState::One);
    assert(engine.Errors().empty());

    const auto text = SaveDocumentToString(doc);
    auto round = LoadDocumentFromString(text);
    assert(round.status && round.document.Main().Components().size() == 4);
    return 0;
}
```

### C. 术语

| 术语 | 含义 |
| --- | --- |
| 元件实例 | 画布上的一个具体元件，有 ID、位置、属性 |
| 引脚 | 元件的连接点，有方向、位宽、本地偏移 |
| 导线 | 用户画的折线，是**物理**实体 |
| 网络 / 网 | 多个导线与引脚连通后的**逻辑**实体，由 `BuildNetlist` 算出 |
| 驱动 | 输出引脚向网络写值 |
| 收敛 | 组合电路迭代求值到不再变化 |
| 网表 | 扁平化后的"网络 ↔ 引脚"结构，仿真直接消费 |
| 提交 | 上升沿时把时序元件的新状态写入 `ComponentState` |

### D. 评审记录

| 日期 | 版本 | 决议 | 参与人 |
| --- | --- | --- | --- |
| 2026-11-05 | v1.0-draft | 初稿，待评审 | — |
| | | | |
