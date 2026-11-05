#include "ll/component/registry.h"

#include "ll/model/logic_value.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ll {
namespace {

constexpr std::uint8_t kMaxWidth = 64;

std::int64_t IntAttr(const AttrMap& attrs, std::string_view key, std::int64_t fallback) {
    const auto it = attrs.find(std::string(key));
    if (it == attrs.end()) return fallback;
    if (const auto* v = std::get_if<std::int64_t>(&it->second)) return *v;
    if (const auto* v = std::get_if<double>(&it->second)) {
        return static_cast<std::int64_t>(std::llround(*v));
    }
    if (const auto* v = std::get_if<bool>(&it->second)) return *v ? 1 : 0;
    return fallback;
}

std::uint8_t WidthAttr(const AttrMap& attrs, std::int64_t fallback = 1) {
    const std::int64_t w = IntAttr(attrs, "width", fallback);
    if (w < 1) return 1;
    if (w > 64) return 64;
    return static_cast<std::uint8_t>(w);
}

std::int64_t ClampInt(std::int64_t v, std::int64_t lo, std::int64_t hi) {
    return std::max(lo, std::min(hi, v));
}

int BitsNeeded(int n) {
    int bits = 0;
    while ((1 << bits) < n) ++bits;
    return std::max(1, bits);
}

AttrDef MakeIntAttr(std::string key, std::string label, std::int64_t def,
                    std::int64_t min, std::int64_t max) {
    AttrDef a;
    a.key = std::move(key);
    a.label = std::move(label);
    a.type = AttrType::Int;
    a.default_value = def;
    a.min = min;
    a.max = max;
    return a;
}

AttrDef MakeWidthAttr(std::int64_t def = 1) {
    return MakeIntAttr("width", "位宽", def, 1, kMaxWidth);
}

std::vector<AttrDef> GateAttrs(std::int64_t inputs = 2) {
    std::vector<AttrDef> out;
    out.push_back(MakeIntAttr("inputs", "输入引脚数", inputs, 2, 16));
    out.push_back(MakeWidthAttr());
    return out;
}

Shape MakeRect(GridRect r, ColorRole stroke = ColorRole::Ink,
               ColorRole fill = ColorRole::Fill, bool filled = true) {
    Shape s;
    s.kind = Shape::Kind::Rect;
    s.rect = r;
    s.stroke = stroke;
    s.fill = fill;
    s.filled = filled;
    return s;
}

Shape MakePolyline(std::vector<GridPoint> pts, ColorRole stroke = ColorRole::Ink,
                   ColorRole fill = ColorRole::Fill, bool filled = false) {
    Shape s;
    s.kind = Shape::Kind::Polyline;
    s.points = std::move(pts);
    s.stroke = stroke;
    s.fill = fill;
    s.filled = filled;
    return s;
}

Shape MakeCircle(GridRect r, ColorRole stroke = ColorRole::Ink,
                 ColorRole fill = ColorRole::Fill, bool filled = true) {
    Shape s;
    s.kind = Shape::Kind::Circle;
    s.rect = r;
    s.stroke = stroke;
    s.fill = fill;
    s.filled = filled;
    return s;
}

Shape MakeText(GridPoint at, std::string text, int size = 12,
               ColorRole color = ColorRole::Ink) {
    Shape s;
    s.kind = Shape::Kind::Text;
    s.rect.tl = at;
    s.text = std::move(text);
    s.font_size = size;
    s.stroke = color;
    s.stroke_width = 0;
    return s;
}

class InputDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "input"; }
    std::string_view Category() const override { return "I/O"; }
    std::string_view DisplayName() const override { return "输入引脚"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        return {{"out", PinDir::Output, WidthAttr(attrs), {20, 0}}};
    }

    void Eval(const EvalContext&, ComponentState*, EvalResult& out) const override {
        if (!out.pins.empty()) out.pins[0] = LogicValue::Zero(out.pins[0].width);
    }

    bool IsInputSource() const override { return true; }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-14, -10}, {12, 10}}));
        dl.shapes.push_back(MakeText({-10, -6}, "IN", 14));
        return dl;
    }
};

class ButtonDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "button"; }
    std::string_view Category() const override { return "I/O"; }
    std::string_view DisplayName() const override { return "按钮"; }

    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"out", PinDir::Output, 1, {20, 0}}};
    }

    void Eval(const EvalContext&, ComponentState*, EvalResult& out) const override {
        if (!out.pins.empty()) out.pins[0] = LogicValue::Zero(1);
    }

    bool IsInputSource() const override { return true; }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-10, -10}, {10, 10}}));
        dl.shapes.push_back(MakeText({-8, -5}, "BTN", 10));
        return dl;
    }
};

class OutputDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "output"; }
    std::string_view Category() const override { return "I/O"; }
    std::string_view DisplayName() const override { return "输出引脚"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        return {{"in", PinDir::Input, WidthAttr(attrs), {-20, 0}}};
    }

    void Eval(const EvalContext&, ComponentState*, EvalResult&) const override {}

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-12, -10}, {14, 10}}));
        dl.shapes.push_back(MakeText({-10, -6}, "OUT", 10));
        return dl;
    }
};

class LedDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "led"; }
    std::string_view Category() const override { return "I/O"; }
    std::string_view DisplayName() const override { return "LED"; }

    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"in", PinDir::Input, 1, {-20, 0}}};
    }

    void Eval(const EvalContext&, ComponentState*, EvalResult&) const override {}

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeCircle({{-9, -9}, {9, 9}}, ColorRole::Ink,
                                       ColorRole::Fill, true));
        dl.shapes.push_back(MakeText({-8, -5}, "LED", 8));
        return dl;
    }

    bool HasLiveSymbol() const override { return true; }

    void BuildLiveSymbol(const EvalContext& ctx, DrawList& dl) const override {
        ColorRole fill = ColorRole::Fill;
        if (!ctx.pins.empty()) {
            switch (ctx.pins[0].Bit(0)) {
                case LogicState::One:     fill = ColorRole::Accent; break;
                case LogicState::Unknown: fill = ColorRole::Error; break;
                default:                  fill = ColorRole::Fill; break;
            }
        }
        for (Shape& s : dl.shapes) {
            if (s.kind == Shape::Kind::Circle) {
                s.fill = fill;
                s.filled = true;
            }
        }
    }
};

class ClockDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "clock"; }
    std::string_view Category() const override { return "I/O"; }
    std::string_view DisplayName() const override { return "时钟"; }

    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"out", PinDir::Output, 1, {20, 0}}};
    }

    void Eval(const EvalContext&, ComponentState*, EvalResult& out) const override {
        if (!out.pins.empty()) out.pins[0] = LogicValue::Zero(1);
    }

    bool IsClockSource() const override { return true; }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakePolyline({{-16, 6}, {-8, 6}, {-8, -6}, {4, -6},
                                          {4, 6}, {12, 6}},
                                         ColorRole::Ink));
        return dl;
    }
};

enum class GateKind { And, Or, Not, Nand, Nor, Xor };

class GateDef final : public ComponentDef {
public:
    explicit GateDef(GateKind kind) : kind_(kind) {}

    std::string_view Type() const override {
        switch (kind_) {
            case GateKind::And:  return "and";
            case GateKind::Or:   return "or";
            case GateKind::Not:  return "not";
            case GateKind::Nand: return "nand";
            case GateKind::Nor:  return "nor";
            case GateKind::Xor:  return "xor";
        }
        return "and";
    }

    std::string_view Category() const override { return "Gates"; }

    std::string_view DisplayName() const override {
        switch (kind_) {
            case GateKind::And:  return "与门";
            case GateKind::Or:   return "或门";
            case GateKind::Not:  return "非门";
            case GateKind::Nand: return "与非门";
            case GateKind::Nor:  return "或非门";
            case GateKind::Xor:  return "异或门";
        }
        return "与门";
    }

    std::vector<AttrDef> Attrs() const override {
        if (kind_ == GateKind::Not) return {MakeWidthAttr()};
        return GateAttrs();
    }

    int InputCount(const AttrMap& attrs) const {
        if (kind_ == GateKind::Not) return 1;
        return static_cast<int>(ClampInt(IntAttr(attrs, "inputs", 2), 2, 16));
    }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const int n = InputCount(attrs);
        const std::uint8_t width = WidthAttr(attrs);
        std::vector<PinDef> pins;
        pins.reserve(static_cast<std::size_t>(n + 1));
        if (kind_ == GateKind::Not) {
            pins.push_back({"in", PinDir::Input, width, {-24, 0}});
        } else {
            for (int i = 0; i < n; ++i) {
                pins.push_back({"in" + std::to_string(i), PinDir::Input, width,
                                {-24, (2 * i - (n - 1)) * 5}});
            }
        }
        pins.push_back({"out", PinDir::Output, width, {24, 0}});
        return pins;
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.empty() || out.pins.empty()) return;
        const int n = static_cast<int>(ctx.pins.size()) - 1;
        const std::uint8_t width = ctx.pins.back().width;

        LogicValue acc;
        switch (kind_) {
            case GateKind::And:
            case GateKind::Nand:
                acc = LogicValue::One(width);
                for (int i = 0; i < n; ++i) acc = LogicAnd(acc, ctx.pins[i]);
                if (kind_ == GateKind::Nand) acc = LogicNot(acc);
                break;
            case GateKind::Or:
            case GateKind::Nor:
                acc = LogicValue::Zero(width);
                for (int i = 0; i < n; ++i) acc = LogicOr(acc, ctx.pins[i]);
                if (kind_ == GateKind::Nor) acc = LogicNot(acc);
                break;
            case GateKind::Xor:
                acc = LogicValue::Zero(width);
                for (int i = 0; i < n; ++i) acc = LogicXor(acc, ctx.pins[i]);
                break;
            case GateKind::Not:
                acc = LogicNot(ctx.pins[0]);
                break;
        }
        out.pins.back() = LogicAdaptWidth(acc, width);
    }

    DrawList BaseSymbol(const AttrMap& attrs) const override {
        const int n = InputCount(attrs);
        const int half = std::max(10, (n - 1) * 5 + 8);
        DrawList dl;
        if (kind_ == GateKind::Not) {
            dl.shapes.push_back(MakePolyline({{-16, -14}, {14, 0}, {-16, 14}, {-16, -14}},
                                             ColorRole::Ink, ColorRole::Fill, true));
            return dl;
        }
        dl.shapes.push_back(MakeRect({{-16, -half}, {16, half}}));
        switch (kind_) {
            case GateKind::And:
            case GateKind::Nand:
                dl.shapes.push_back(MakeText({-8, -6}, "&", 14));
                break;
            case GateKind::Or:
            case GateKind::Nor:
                dl.shapes.push_back(MakeText({-8, -6}, ">=1", 12));
                break;
            case GateKind::Xor:
                dl.shapes.push_back(MakeText({-8, -6}, "=1", 14));
                break;
            case GateKind::Not:
                break;
        }
        if (kind_ == GateKind::Nand || kind_ == GateKind::Nor) {
            dl.shapes.push_back(MakeCircle({{16, -4}, {24, 4}}));
        }
        return dl;
    }

private:
    GateKind kind_;
};

struct DffState final : ComponentState {
    LogicValue q = LogicValue::Zero(1);
};

class DffDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "dff"; }
    std::string_view Category() const override { return "Memory"; }
    std::string_view DisplayName() const override { return "D 触发器"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const std::uint8_t w = WidthAttr(attrs);
        return {{"d", PinDir::Input, w, {-24, 0}},
                {"q", PinDir::Output, w, {24, -6}},
                {"qbar", PinDir::Output, w, {24, 6}}};
    }

    bool IsSequential() const override { return true; }

    std::unique_ptr<ComponentState> CreateState() const override {
        return std::make_unique<DffState>();
    }

    void Reset(ComponentState* state) const override {
        if (auto* s = static_cast<DffState*>(state)) s->q = LogicValue::Zero(1);
    }

    void Eval(const EvalContext& ctx, ComponentState* state, EvalResult& out) const override {
        if (ctx.pins.size() < 3 || out.pins.size() < 3) return;
        auto* s = static_cast<DffState*>(state);
        const std::uint8_t w = ctx.pins[1].width;
        const LogicValue q = s ? LogicAdaptWidth(s->q, w) : LogicValue::Unknown(w);
        out.pins[1] = q;
        out.pins[2] = LogicNot(q);
    }

    void CommitEdge(const EvalContext& ctx, ComponentState* state) const override {
        auto* s = static_cast<DffState*>(state);
        if (!s || ctx.pins.size() < 2) return;
        s->q = LogicAdaptWidth(ctx.pins[0], ctx.pins[1].width);
    }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-16, -16}, {16, 16}}));
        dl.shapes.push_back(MakeText({-10, -6}, "D", 14));
        return dl;
    }
};

struct RegisterState final : ComponentState {
    LogicValue q = LogicValue::Zero(1);
};

class RegisterDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "register"; }
    std::string_view Category() const override { return "Memory"; }
    std::string_view DisplayName() const override { return "寄存器"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const std::uint8_t w = WidthAttr(attrs);
        return {{"d", PinDir::Input, w, {-24, 0}},
                {"q", PinDir::Output, w, {24, 0}}};
    }

    bool IsSequential() const override { return true; }

    std::unique_ptr<ComponentState> CreateState() const override {
        return std::make_unique<RegisterState>();
    }

    void Reset(ComponentState* state) const override {
        if (auto* s = static_cast<RegisterState*>(state)) s->q = LogicValue::Zero(1);
    }

    void Eval(const EvalContext& ctx, ComponentState* state, EvalResult& out) const override {
        if (ctx.pins.size() < 2 || out.pins.size() < 2) return;
        auto* s = static_cast<RegisterState*>(state);
        const std::uint8_t w = ctx.pins[1].width;
        out.pins[1] = s ? LogicAdaptWidth(s->q, w) : LogicValue::Unknown(w);
    }

    void CommitEdge(const EvalContext& ctx, ComponentState* state) const override {
        auto* s = static_cast<RegisterState*>(state);
        if (!s || ctx.pins.size() < 2) return;
        s->q = LogicAdaptWidth(ctx.pins[0], ctx.pins[1].width);
    }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-18, -12}, {18, 12}}));
        dl.shapes.push_back(MakeText({-14, -6}, "REG", 10));
        return dl;
    }
};

struct CounterState final : ComponentState {
    std::uint64_t count = 0;
};

class CounterDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "counter"; }
    std::string_view Category() const override { return "Memory"; }
    std::string_view DisplayName() const override { return "计数器"; }

    std::vector<AttrDef> Attrs() const override {
        std::vector<AttrDef> out;
        out.push_back(MakeIntAttr("bits", "输出位宽", 4, 1, 16));
        out.push_back(MakeIntAttr("modulus", "模值", 16, 1, 65535));
        return out;
    }

    std::uint8_t Bits(const AttrMap& attrs) const {
        return static_cast<std::uint8_t>(ClampInt(IntAttr(attrs, "bits", 4), 1, 16));
    }

    std::uint64_t Modulus(const AttrMap& attrs) const {
        const std::uint64_t bits = Bits(attrs);
        const std::uint64_t limit = std::uint64_t{1} << bits;
        return static_cast<std::uint64_t>(
            ClampInt(IntAttr(attrs, "modulus", static_cast<std::int64_t>(limit)), 1,
                     static_cast<std::int64_t>(limit)));
    }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        return {{"q", PinDir::Output, Bits(attrs), {24, 0}}};
    }

    bool IsSequential() const override { return true; }

    std::unique_ptr<ComponentState> CreateState() const override {
        return std::make_unique<CounterState>();
    }

    void Reset(ComponentState* state) const override {
        if (auto* s = static_cast<CounterState*>(state)) s->count = 0;
    }

    void Eval(const EvalContext& ctx, ComponentState* state, EvalResult& out) const override {
        if (ctx.pins.empty() || out.pins.empty()) return;
        auto* s = static_cast<CounterState*>(state);
        out.pins[0] = LogicValue::FromUInt(s ? s->count : 0, ctx.pins[0].width);
    }

    void CommitEdge(const EvalContext& ctx, ComponentState* state) const override {
        auto* s = static_cast<CounterState*>(state);
        if (!s) return;
        const std::uint64_t mod = Modulus(ctx.attrs);
        if (mod == 0) return;
        s->count = (s->count + 1) % mod;
    }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-18, -12}, {18, 12}}));
        dl.shapes.push_back(MakeText({-16, -6}, "CTR", 10));
        return dl;
    }
};

class MuxDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "mux"; }
    std::string_view Category() const override { return "Arithmetic"; }
    std::string_view DisplayName() const override { return "多路选择器"; }

    std::vector<AttrDef> Attrs() const override {
        std::vector<AttrDef> out;
        AttrDef inputs = MakeIntAttr("inputs", "数据输入数", 2, 2, 16);
        inputs.options = {{"2", "2"}, {"4", "4"}, {"8", "8"}, {"16", "16"}};
        out.push_back(inputs);
        out.push_back(MakeWidthAttr());
        return out;
    }

    int Inputs(const AttrMap& attrs) const {
        return static_cast<int>(ClampInt(IntAttr(attrs, "inputs", 2), 2, 16));
    }

    int SelWidth(const AttrMap& attrs) const { return BitsNeeded(Inputs(attrs)); }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const int n = Inputs(attrs);
        const std::uint8_t w = WidthAttr(attrs);
        const int sel_w = SelWidth(attrs);
        std::vector<PinDef> pins;
        pins.reserve(static_cast<std::size_t>(n + 2));
        for (int i = 0; i < n; ++i) {
            pins.push_back({"d" + std::to_string(i), PinDir::Input, w,
                            {-28, (2 * i - (n - 1)) * 4}});
        }
        pins.push_back({"sel", PinDir::Input,
                        static_cast<std::uint8_t>(sel_w), {-28, (n + 1) * 4}});
        pins.push_back({"out", PinDir::Output, w, {28, 0}});
        return pins;
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.size() < 3 || out.pins.empty()) return;
        const int n = static_cast<int>(ctx.pins.size()) - 2;
        const LogicValue sel = ctx.pins[static_cast<std::size_t>(n)];
        const std::uint8_t w = out.pins.back().width;
        if (!sel.AllKnown()) {
            out.pins.back() = LogicValue::Unknown(w);
            return;
        }
        const std::uint64_t idx = sel.value;
        if (idx >= static_cast<std::uint64_t>(n)) {
            out.pins.back() = LogicValue::Unknown(w);
            return;
        }
        out.pins.back() = LogicAdaptWidth(ctx.pins[static_cast<std::size_t>(idx)], w);
    }

    DrawList BaseSymbol(const AttrMap& attrs) const override {
        const int n = Inputs(attrs);
        const int half = std::max(14, (n + 1) * 4);
        DrawList dl;
        dl.shapes.push_back(MakePolyline({{-18, -half}, {10, -half}, {20, 0},
                                          {10, half}, {-18, half}, {-18, -half}},
                                         ColorRole::Ink, ColorRole::Fill, true));
        dl.shapes.push_back(MakeText({-14, -6}, "MUX", 10));
        return dl;
    }
};

class DecoderDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "decoder"; }
    std::string_view Category() const override { return "Arithmetic"; }
    std::string_view DisplayName() const override { return "译码器"; }

    std::vector<AttrDef> Attrs() const override {
        std::vector<AttrDef> out;
        AttrDef outputs = MakeIntAttr("outputs", "输出数", 2, 2, 16);
        outputs.options = {{"2", "2"}, {"4", "4"}, {"8", "8"}, {"16", "16"}};
        out.push_back(outputs);
        return out;
    }

    int Outputs(const AttrMap& attrs) const {
        return static_cast<int>(ClampInt(IntAttr(attrs, "outputs", 2), 2, 16));
    }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const int n = Outputs(attrs);
        const int sel_w = BitsNeeded(n);
        std::vector<PinDef> pins;
        pins.reserve(static_cast<std::size_t>(n + 1));
        pins.push_back({"sel", PinDir::Input, static_cast<std::uint8_t>(sel_w), {-24, 0}});
        for (int i = 0; i < n; ++i) {
            pins.push_back({"out" + std::to_string(i), PinDir::Output, 1,
                            {24, (2 * i - (n - 1)) * 4}});
        }
        return pins;
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.empty() || out.pins.size() < 2) return;
        const std::size_t n = out.pins.size() - 1;
        const LogicValue sel = ctx.pins[0];
        if (!sel.AllKnown()) {
            for (std::size_t i = 0; i < n; ++i) out.pins[i + 1] = LogicValue::Unknown(1);
            return;
        }
        for (std::size_t i = 0; i < n; ++i) {
            out.pins[i + 1] = LogicValue::FromUInt(i == sel.value ? 1 : 0, 1);
        }
    }

    DrawList BaseSymbol(const AttrMap& attrs) const override {
        const int n = Outputs(attrs);
        const int half = std::max(14, (n - 1) * 4 + 8);
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-18, -half}, {18, half}}));
        dl.shapes.push_back(MakeText({-16, -6}, "DEC", 10));
        return dl;
    }
};

class AdderDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "adder"; }
    std::string_view Category() const override { return "Arithmetic"; }
    std::string_view DisplayName() const override { return "加法器"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const std::uint8_t w = WidthAttr(attrs);
        return {{"a", PinDir::Input, w, {-28, -8}},
                {"b", PinDir::Input, w, {-28, 8}},
                {"cin", PinDir::Input, 1, {-28, 24}},
                {"sum", PinDir::Output, w, {28, 0}},
                {"cout", PinDir::Output, 1, {28, 24}}};
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.size() < 5 || out.pins.size() < 5) return;
        if (!ctx.pins[0].AllKnown() || !ctx.pins[1].AllKnown() || !ctx.pins[2].AllKnown()) {
            out.pins[3] = LogicValue::Unknown(out.pins[3].width);
            out.pins[4] = LogicValue::Unknown(1);
            return;
        }
        const std::uint8_t w = out.pins[3].width;
        const std::uint64_t mask = LogicMask(w);
        const std::uint64_t a = ctx.pins[0].value & mask;
        const std::uint64_t b = ctx.pins[1].value & mask;
        const std::uint64_t cin = ctx.pins[2].value & 1;
        const std::uint64_t s1 = a + b;
        const bool c1 = s1 < a;
        const std::uint64_t s2 = s1 + cin;
        const bool c2 = s2 < s1;
        out.pins[3] = LogicValue::FromUInt(s2 & mask, w);
        out.pins[4] = LogicValue::FromUInt((c1 || c2) ? 1 : 0, 1);
    }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-20, -16}, {20, 16}}));
        dl.shapes.push_back(MakeText({-14, -6}, "ADD", 10));
        return dl;
    }
};

class ComparatorDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "comparator"; }
    std::string_view Category() const override { return "Arithmetic"; }
    std::string_view DisplayName() const override { return "比较器"; }

    std::vector<AttrDef> Attrs() const override { return {MakeWidthAttr()}; }

    std::vector<PinDef> Pins(const AttrMap& attrs) const override {
        const std::uint8_t w = WidthAttr(attrs);
        return {{"a", PinDir::Input, w, {-28, -8}},
                {"b", PinDir::Input, w, {-28, 8}},
                {"eq", PinDir::Output, 1, {28, -12}},
                {"gt", PinDir::Output, 1, {28, 0}},
                {"lt", PinDir::Output, 1, {28, 12}}};
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.size() < 5 || out.pins.size() < 5) return;
        if (!ctx.pins[0].AllKnown() || !ctx.pins[1].AllKnown()) {
            out.pins[2] = LogicValue::Unknown(1);
            out.pins[3] = LogicValue::Unknown(1);
            out.pins[4] = LogicValue::Unknown(1);
            return;
        }
        const std::uint64_t a = ctx.pins[0].value;
        const std::uint64_t b = ctx.pins[1].value;
        out.pins[2] = LogicValue::FromUInt(a == b ? 1 : 0, 1);
        out.pins[3] = LogicValue::FromUInt(a > b ? 1 : 0, 1);
        out.pins[4] = LogicValue::FromUInt(a < b ? 1 : 0, 1);
    }

    DrawList BaseSymbol(const AttrMap&) const override {
        DrawList dl;
        dl.shapes.push_back(MakeRect({{-20, -18}, {20, 18}}));
        dl.shapes.push_back(MakeText({-16, -6}, "CMP", 10));
        return dl;
    }
};

}  // namespace

SubcircuitDef::SubcircuitDef(CircuitId target, std::string type_name)
    : target_(target), type_name_(std::move(type_name)) {}

std::vector<PinDef> SubcircuitDef::Pins(const AttrMap&) const {
    std::vector<PinDef> pins;
    pins.reserve(ports_.size());
    for (const Port& p : ports_) {
        pins.push_back({p.name, p.dir, p.width, p.position});
    }
    return pins;
}

void SubcircuitDef::Eval(const EvalContext&, ComponentState*, EvalResult&) const {
    assert(false && "SubcircuitDef::Eval must never be called");
}

DrawList SubcircuitDef::BaseSymbol(const AttrMap&) const {
    int min_x = -16, max_x = 16, min_y = -12, max_y = 12;
    if (!ports_.empty()) {
        min_x = ports_.front().position.x;
        max_x = min_x;
        min_y = ports_.front().position.y;
        max_y = min_y;
        for (const Port& p : ports_) {
            min_x = std::min(min_x, p.position.x);
            max_x = std::max(max_x, p.position.x);
            min_y = std::min(min_y, p.position.y);
            max_y = std::max(max_y, p.position.y);
        }
        min_x -= 8;
        max_x += 8;
        min_y -= 8;
        max_y += 8;
    }
    DrawList dl;
    dl.shapes.push_back(MakeRect({{min_x, min_y}, {max_x, max_y}}));
    dl.shapes.push_back(
        MakeText({min_x + 4, (min_y + max_y) / 2 - 5}, type_name_, 10));
    return dl;
}

const ComponentRegistry& BuiltinComponents() {
    static const ComponentRegistry registry = [] {
        ComponentRegistry r;
        r.Register(std::make_unique<InputDef>());
        r.Register(std::make_unique<ButtonDef>());
        r.Register(std::make_unique<OutputDef>());
        r.Register(std::make_unique<LedDef>());
        r.Register(std::make_unique<ClockDef>());
        r.Register(std::make_unique<GateDef>(GateKind::And));
        r.Register(std::make_unique<GateDef>(GateKind::Or));
        r.Register(std::make_unique<GateDef>(GateKind::Not));
        r.Register(std::make_unique<GateDef>(GateKind::Nand));
        r.Register(std::make_unique<GateDef>(GateKind::Nor));
        r.Register(std::make_unique<GateDef>(GateKind::Xor));
        r.Register(std::make_unique<DffDef>());
        r.Register(std::make_unique<RegisterDef>());
        r.Register(std::make_unique<CounterDef>());
        r.Register(std::make_unique<MuxDef>());
        r.Register(std::make_unique<DecoderDef>());
        r.Register(std::make_unique<AdderDef>());
        r.Register(std::make_unique<ComparatorDef>());
        return r;
    }();
    return registry;
}

}  // namespace ll
