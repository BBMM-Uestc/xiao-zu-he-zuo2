#include "test_framework.h"

#include "fixture.h"
#include "ll/component/registry.h"
#include "ll/sim/engine.h"

#include <memory>

using namespace ll;

namespace {

bool HasError(const sim::Engine& engine, sim::NetlistError::Kind kind) {
    for (const auto& error : engine.Errors()) {
        if (error.kind == kind) return true;
    }
    return false;
}

class ToggleDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "toggle"; }
    std::string_view Category() const override { return "Test"; }
    std::string_view DisplayName() const override { return "振荡测试元件"; }

    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"out", PinDir::Output, 1, {0, 0}}};
    }

    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (out.pins.empty()) return;
        const bool high = !ctx.pins.empty() && ctx.pins[0].Bit(0) == LogicState::One;
        out.pins[0] = high ? LogicValue::Zero(1) : LogicValue::One(1);
    }

    DrawList BaseSymbol(const AttrMap&) const override { return {}; }
};

}  // namespace

LL_TEST(diagnostic_floating_input) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId g = fixture::Add(c, "and", {40, 20});
    const ComponentId led = fixture::Add(c, "led", {90, 20});
    fixture::Connect(c, {a, 0}, {g, 0});
    fixture::Connect(c, {g, 2}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK(HasError(engine, sim::NetlistError::Kind::FloatingInput));
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Zero);
}

LL_TEST(diagnostic_multiple_drivers) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId b = fixture::Add(c, "input", {0, 40});
    const ComponentId led = fixture::Add(c, "led", {100, 20});
    fixture::Connect(c, {a, 0}, {led, 0});
    fixture::Connect(c, {b, 0}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    engine.SetInput({a, 0}, LogicValue::Zero(1));
    engine.SetInput({b, 0}, LogicValue::One(1));
    engine.Tick();

    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Unknown);
    CHECK(HasError(engine, sim::NetlistError::Kind::MultipleDrivers));
}

LL_TEST(diagnostic_combinational_oscillation) {
    ComponentRegistry registry;
    registry.Register(std::make_unique<ToggleDef>());

    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    fixture::Add(c, "toggle", {0, 0});

    sim::Engine engine(registry);
    REQUIRE(engine.Load(doc));
    CHECK(HasError(engine, sim::NetlistError::Kind::Oscillation));

    bool converged = true;
    engine.SetListener([&](const sim::SimFrame& frame) { converged = frame.converged; });
    engine.Tick();
    CHECK(!converged);
}

LL_TEST(diagnostic_width_mismatch) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId a = fixture::Add(c, "input", {0, 0}, {{"width", std::int64_t{8}}});
    const ComponentId g = fixture::Add(
        c, "and", {40, 20},
        {{"inputs", std::int64_t{2}}, {"width", std::int64_t{4}}});
    fixture::Connect(c, {a, 0}, {g, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK(HasError(engine, sim::NetlistError::Kind::WidthMismatch));
}
