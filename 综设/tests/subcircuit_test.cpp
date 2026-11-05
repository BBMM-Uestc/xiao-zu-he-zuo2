#include "test_framework.h"

#include "fixture.h"
#include "ll/component/registry.h"
#include "ll/sim/engine.h"

#include <memory>

using namespace ll;

namespace {

class TestInputDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "tin"; }
    std::string_view Category() const override { return "Test"; }
    std::string_view DisplayName() const override { return "tin"; }
    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"out", PinDir::Output, 1, {0, 0}}};
    }
    void Eval(const EvalContext&, ComponentState*, EvalResult& out) const override {
        if (!out.pins.empty()) out.pins[0] = LogicValue::Zero(1);
    }
    bool IsInputSource() const override { return true; }
    DrawList BaseSymbol(const AttrMap&) const override { return {}; }
};

class TestLedDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "tled"; }
    std::string_view Category() const override { return "Test"; }
    std::string_view DisplayName() const override { return "tled"; }
    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"in", PinDir::Input, 1, {0, 0}}};
    }
    void Eval(const EvalContext&, ComponentState*, EvalResult&) const override {}
    DrawList BaseSymbol(const AttrMap&) const override { return {}; }
};

class TestNotDef final : public ComponentDef {
public:
    std::string_view Type() const override { return "tnot"; }
    std::string_view Category() const override { return "Test"; }
    std::string_view DisplayName() const override { return "tnot"; }
    std::vector<PinDef> Pins(const AttrMap&) const override {
        return {{"in", PinDir::Input, 1, {-10, 0}},
                {"out", PinDir::Output, 1, {10, 0}}};
    }
    void Eval(const EvalContext& ctx, ComponentState*, EvalResult& out) const override {
        if (ctx.pins.size() >= 2 && out.pins.size() >= 2) {
            out.pins[1] = LogicNot(ctx.pins[0]);
        }
    }
    DrawList BaseSymbol(const AttrMap&) const override { return {}; }
};

}  // namespace

LL_TEST(subcircuit_flattens_and_simulates) {
    Document doc = Document::CreateEmpty();
    Circuit& sub = doc.CreateCircuit("inv");

    sub.AddPort(Port{"in", PinDir::Input, 1, {0, 0}});
    sub.AddPort(Port{"out", PinDir::Output, 1, {40, 0}});
    const ComponentId not_gate = fixture::Add(sub, "tnot", {20, 0});

    {
        WireSpec spec;
        spec.points = {{0, 0}, {10, 0}};
        spec.from.attached = false;
        spec.from.point = {0, 0};
        spec.to.attached = true;
        spec.to.pin = {not_gate, 0};
        sub.AddWire(spec);
    }
    {
        WireSpec spec;
        spec.points = {{30, 0}, {40, 0}};
        spec.from.attached = true;
        spec.from.pin = {not_gate, 1};
        spec.to.attached = false;
        spec.to.point = {40, 0};
        sub.AddWire(spec);
    }

    Circuit& main = doc.Main();
    const ComponentId in = main.AddComponent({"tin", {0, 100}, 0, false, {}, ""});
    auto sub_def = std::make_unique<SubcircuitDef>(sub.Id(), "inv");
    sub_def->SetPorts(sub.Ports());
    const ComponentId inst = main.AddComponent({"inv", {100, 100}, 0, false, {}, ""});
    const ComponentId led = main.AddComponent({"tled", {200, 100}, 0, false, {}, ""});

    {
        WireSpec spec;
        spec.points = {{0, 100}, {100, 100}};
        spec.from.attached = true;
        spec.from.pin = {in, 0};
        spec.to.attached = true;
        spec.to.pin = {inst, 0};
        main.AddWire(spec);
    }
    {
        WireSpec spec;
        spec.points = {{140, 100}, {200, 100}};
        spec.from.attached = true;
        spec.from.pin = {inst, 1};
        spec.to.attached = true;
        spec.to.pin = {led, 0};
        main.AddWire(spec);
    }

    ComponentRegistry registry;
    registry.Register(std::make_unique<TestInputDef>());
    registry.Register(std::make_unique<TestLedDef>());
    registry.Register(std::make_unique<TestNotDef>());
    registry.Register(std::move(sub_def));

    sim::Engine engine(registry);
    REQUIRE(engine.Load(doc));

    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::One);
    CHECK(engine.Value({inst, 0}).Bit(0) == LogicState::Zero);
    CHECK(engine.Value({inst, 1}).Bit(0) == LogicState::One);

    engine.SetInput({in, 0}, LogicValue::One(1));
    engine.Tick();
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Zero);
    CHECK(engine.Errors().empty());
}

LL_TEST(subcircuit_cycle_is_rejected) {
    Document doc = Document::CreateEmpty();
    Circuit& main = doc.Main();
    main.AddPort(Port{"in", PinDir::Input, 1, {0, 0}});

    auto sub_def = std::make_unique<SubcircuitDef>(main.Id(), "self");
    sub_def->SetPorts(main.Ports());
    main.AddComponent({"self", {50, 0}, 0, false, {}, ""});

    ComponentRegistry registry;
    registry.Register(std::move(sub_def));

    sim::Engine engine(registry);
    const Status status = engine.Load(doc);
    CHECK(!status);
    CHECK(status.code == StatusCode::CycleDetected);
}

