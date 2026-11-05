# logisim-lite · C 元件库起步工程

> 这是「元件库（C）」负责人的起点：一个能编译、能跑测试、能直接加元件的 VS 解决方案。
> 双击 `C_Starter.sln` → 选 `Debug | x64` → **F5**，应该看到 `12 case(s), 0 failed`。

---

## 1. 快速开始

### 图形界面

1. 打开 `c-starter\C_Starter.sln`
2. 顶部配置选 **Debug**，平台选 **x64**
3. **F5**（或 Ctrl+F5）

### 命令行

```bat
cd c-starter
msbuild C_Starter.sln /p:Configuration=Debug /p:Platform=x64
bin\x64\Debug\tests_component.exe
```

> `msbuild` 在「Developer PowerShell for VS 2026」或「x64 Native Tools 命令提示」里可用。

---

## 2. 目录结构

```
c-starter/
├── C_Starter.sln                 # 解决方案（VS 2022 / 2026 都能开）
├── model/                        # ll_model：D 负责，这里是"最小桩"
│   ├── ll_model.vcxproj
│   ├── include/ll/model/
│   │   ├── types.h               # ID、GridPoint、PinRef、AttrValue、Status
│   │   ├── logic_value.h         # 三态值
│   │   └── circuit.h             # ComponentInstance（桩）
│   └── src/
│       ├── logic_value.cpp
│       └── model_version.cpp
├── component/                    # ll_component：C 的主战场
│   ├── ll_component.vcxproj
│   ├── include/ll/component/
│   │   ├── defs.h                # PinDef / AttrDef / Shape / DrawList
│   │   ├── component_def.h       # ComponentDef 基类 + EvalContext/EvalResult
│   │   ├── registry.h            # ComponentRegistry + BuiltinComponents()
│   │   ├── geometry.h            # PinWorldPos / ComponentBounds
│   │   └── builtin.h             # 内置元件工厂
│   └── src/
│       ├── attr_util.h           # 内部小工具（读属性、造 AttrDef）
│       ├── and_gate.cpp          # 与门
│       ├── input.cpp             # 输入引脚
│       ├── led.cpp               # LED
│       ├── registry.cpp          # 注册表 + 内置元件装配
│       └── geometry.cpp          # 引脚坐标变换、包围盒
├── tests/                        # 自研零依赖测试框架
│   ├── test_main.h               # LL_TEST / LL_CHECK / LL_CHECK_EQ / LL_REQUIRE
│   └── test_util.h               # EvalWithInputs 辅助
└── tests_component/              # 测试可执行程序
    ├── tests_component.vcxproj
    ├── test_main.cpp
    ├── logic_value_test.cpp
    ├── and_gate_test.cpp
    ├── led_test.cpp
    └── geometry_registry_test.cpp
```

产物统一输出到 `c-starter\bin\x64\<Config>\`，中间文件在 `c-starter\obj\`，不进 git。

---

## 3. 三个项目

| 项目 | 类型 | 职责 | 工具集 |
| --- | --- | --- | --- |
| `ll_model` | 静态库 | 基础类型 + 逻辑值（D 的桩，后续扩展 Circuit/Document/JSON） | VS2022 → v143，VS2026 → v145 |
| `ll_component` | 静态库 | 元件定义、求值、图元、注册表、几何 | 同上 |
| `tests_component` | 控制台 | 单元测试 | 同上 |

工具集是**条件选择**的，写死在 vcxproj 的 Configuration 组里：

```xml
<PlatformToolset Condition="'$(VisualStudioVersion)' != '17.0'">v145</PlatformToolset>
<PlatformToolset Condition="'$(VisualStudioVersion)' == '17.0'">v143</PlatformToolset>
```

所以**一个 .sln 两边都能打开、各自用本机工具集构建**，不需要装对方版本。
细节见 `docs/COMPAT.md`。

---

## 4. 已经实现的东西

- `LogicValue`：64 位以内、三态（0/1/X/Z），按位掩码表示；支持 `Bit/Bits/Zext/Trunc`，以及三值 `And/Or/Xor/Not`。
- 三个元件：
  - `input`：属性 `width`(1–8)、`value`(bool)，输出恒定电平
  - `and`：属性 `inputs`(2–8)、`width`(1–8)，逐位与
  - `led`：1 个输入，`BuildLiveSymbol` 在点亮时追加高亮圆
- `ComponentRegistry`：按类型查找、按分类列举、保持注册顺序。
- `PinWorldPos` / `ComponentBounds`：镜像→旋转→平移，编辑器与仿真共用。
- 12 个单元测试，Debug/Release 都通过；并用 MSVC 19.44（VS 2022 17.14 同代）全量编译+链接+跑测通过。

---

## 5. 怎么加一个新元件（以 OR 门为例）

**第 1 步**：新建 `component/src/or_gate.cpp`，照 `and_gate.cpp` 抄一份：

```cpp
class OrGateDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "or"; }
    std::string_view Category() const override { return "Gates"; }
    std::string_view DisplayName() const override { return "或门"; }

    std::vector<AttrDef> Attrs() const override { /* 同 AndGateDef */ }
    std::vector<PinDef>  Pins(const AttrMap& attrs) const override { /* 同 AndGateDef */ }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        const std::size_t n = ctx.pins.size();
        if (n < 2) return;
        LogicValue acc = ctx.pins[0];
        for (std::size_t i = 1; i + 1 < n; ++i) acc = Or(acc, ctx.pins[i]);
        out.pins[n - 1] = acc;
    }

    DrawList BaseSymbol(const AttrMap& attrs) const override { /* 同 AndGateDef，改文字 */ }
};
```

**第 2 步**：`builtin.h` 加一行声明

```cpp
std::unique_ptr<ComponentDef> MakeOrGate();
```

**第 3 步**：`registry.cpp` 的 `BuiltinComponents()` 里加一行

```cpp
r.Register(MakeOrGate());
```

**第 4 步**：把 `or_gate.cpp` 加进 `component\ll_component.vcxproj` 的 `<ItemGroup><ClCompile>`（由 D 统一改工程文件）。

**第 5 步**：在 `tests_component\` 加测试（照 `and_gate_test.cpp`），并加到 `tests_component.vcxproj`。

---

## 6. 怎么写测试

```cpp
#include "test_main.h"
#include "test_util.h"
#include "ll/component/registry.h"

using namespace ll;

LL_TEST(my_component_works) {
    const ComponentDef* def = BuiltinComponents().Find("or");
    LL_REQUIRE(def != nullptr);

    AttrMap attrs;
    attrs["inputs"] = std::int64_t{2};

    const auto out = test::EvalWithInputs(*def, attrs,
                                          {LogicValue::Zero(), LogicValue::One()});
    LL_CHECK(out.back().Bit(0) == LogicState::One);
}
```

| 宏 | 作用 |
| --- | --- |
| `LL_TEST(name)` | 定义一个测试用例，自动注册 |
| `LL_CHECK(cond)` | 失败后**继续**执行，记录一行 |
| `LL_CHECK_EQ(a, b)` | 同上，带表达式文本 |
| `LL_REQUIRE(cond)` | 失败则**中止**当前用例 |

`test::EvalWithInputs(def, attrs, {输入...})` 按引脚声明顺序填输入，返回**全部引脚值**（输出在最后）。

---

## 7. 兼容性说明

- 源码严格 C++20 子集，未使用任何 C++23 设施，未使用 `std::format`，未按 `_MSC_VER` 分叉。
- 编译选项：`/std:c++20 /permissive- /W4 /utf-8 /Zc:__cplusplus`，`SDL 检查=否`。
- 头文件自带闸门：

```cpp
#if !defined(_MSVC_LANG) || _MSVC_LANG < 202002L
#error "本项目要求 /std:c++20"
#endif
```

- 已在 `cl 19.44.35229`（VS 2022 17.14 同代）下全量编译、链接、跑测通过，`/W4` 零警告。
- **不要**新增第三方头文件依赖（本机离线，vcpkg/GitHub 都不可用）。

---

## 8. 与接口文档的对应关系

| 接口文档（`docs/INTERFACES.md`） | 本工程 | 说明 |
| --- | --- | --- |
| §4 `types.h` | ✅ 已实现 | `PinRef` 放在 `types.h`（文档里在 `circuit.h`），方便 component 层直接引用 |
| §4.1 `LogicValue` | ✅ 已实现 | 三掩码 + 三值运算 |
| §5 `Circuit` / `Document` | ⚠️ 桩 | 只给了 `ComponentInstance`，等 D 补 |
| §6 `ComponentDef` | ✅ 已实现 | 含时序钩子，默认空实现 |
| §6.3 `DrawList` | ✅ 已实现 | 纯数据，无 wx 依赖 |
| §6.4 `ComponentRegistry` | ✅ 已实现 | |
| §7 `Engine` | ❌ 未实现 | B 负责，C 暂时用 `EvalWithInputs` 直接调 `Eval` |
| §8 `Command` | ❌ 未实现 | A 负责 |

---

## 9. 下一步建议

1. 补 `or` / `not` / `nand` / `nor` / `xor`，把 `and_gate.cpp` 抽成公共的「多输入门」基类，减少复制粘贴。
2. 补 `clock` / `dff`：等 B 的 `Engine` 接口冻结后再写（`CommitEdge` 语义见接口文档 §6.6）。
3. 属性 `AttrDef` 补齐 `Enum`（如 LED 颜色），A 的属性面板会直接吃这些声明。
4. 每次提交前用 19.44 预检：

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.44
cl /nologo /c /std:c++20 /permissive- /EHsc /W4 /utf-8 /Zc:__cplusplus ^
   /I model\include /I component\include component\src\and_gate.cpp
```
