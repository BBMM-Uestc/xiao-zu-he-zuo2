#include "test_framework.h"

#include "fixture.h"
#include "ll/sim/engine.h"

#include <vector>

using namespace ll;

namespace {

struct AndLed {
    Document doc;
    ComponentId a = 0;
    ComponentId b = 0;
    ComponentId g = 0;
    ComponentId led = 0;
};

AndLed MakeAndLed() {
    AndLed out;
    out.doc = Document::CreateEmpty();
    Circuit& c = out.doc.Main();
    out.a = fixture::Add(c, "input", {0, 0});
    out.b = fixture::Add(c, "input", {0, 20});
    out.g = fixture::Add(c, "and", {40, 20}, {{"inputs", std::int64_t{2}}});
    out.led = fixture::Add(c, "led", {90, 20});
    fixture::Connect(c, {out.a, 0}, {out.g, 0});
    fixture::Connect(c, {out.b, 0}, {out.g, 1});
    fixture::Connect(c, {out.g, 2}, {out.led, 0});
    return out;
}

}  // namespace

LL_TEST(engine_and_truth_table) {
    AndLed circuit = MakeAndLed();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(circuit.doc));

    const LogicState expected[4] = {LogicState::Zero, LogicState::Zero,
                                    LogicState::Zero, LogicState::One};
    for (int bits = 0; bits < 4; ++bits) {
        engine.SetInput({circuit.a, 0}, LogicValue::FromUInt((bits >> 1) & 1, 1));
        engine.SetInput({circuit.b, 0}, LogicValue::FromUInt(bits & 1, 1));
        engine.Tick();
        CHECK(engine.Value({circuit.led, 0}).Bit(0) == expected[bits]);
    }

    CHECK(engine.Errors().empty());
    CHECK_EQ(engine.TickCount(), 4u);
}

LL_TEST(engine_reset_and_input_defaults) {
    AndLed circuit = MakeAndLed();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(circuit.doc));

    engine.SetInput({circuit.a, 0}, LogicValue::One(1));
    engine.SetInput({circuit.b, 0}, LogicValue::One(1));
    engine.Tick();
    CHECK(engine.Value({circuit.led, 0}).Bit(0) == LogicState::One);

    engine.Reset();
    CHECK_EQ(engine.TickCount(), 0u);
    CHECK(engine.Value({circuit.led, 0}).Bit(0) == LogicState::Zero);
}

LL_TEST(engine_reset_clears_attr_initialized_input) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId in = fixture::Add(c, "input", {0, 0}, {{"value", true}});
    const ComponentId led = fixture::Add(c, "led", {70, 0});
    fixture::Connect(c, {in, 0}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::One);

    engine.Reset();
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Zero);
}

LL_TEST(engine_net_value_matches_pin_value) {
    AndLed circuit = MakeAndLed();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(circuit.doc));

    engine.SetInput({circuit.a, 0}, LogicValue::One(1));
    engine.SetInput({circuit.b, 0}, LogicValue::One(1));
    engine.Tick();

    const sim::NetId net = engine.NetlistRef().NetOf({circuit.led, 0});
    CHECK(net.valid());
    CHECK_EQ(engine.NetValue(net).value, 1u);
    CHECK_EQ(engine.Value({circuit.led, 0}).value, 1u);
}

LL_TEST(engine_listener_fires_once_per_tick) {
    AndLed circuit = MakeAndLed();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(circuit.doc));

    std::vector<std::uint64_t> ticks;
    bool saw_change = false;
    engine.SetListener([&](const sim::SimFrame& frame) {
        ticks.push_back(frame.tick);
        if (!frame.changes.empty()) saw_change = true;
        CHECK(frame.converged);
    });

    engine.SetInput({circuit.a, 0}, LogicValue::One(1));
    engine.SetInput({circuit.b, 0}, LogicValue::One(1));
    engine.Advance(3);

    CHECK_EQ(ticks.size(), 3u);
    CHECK_EQ(ticks[0], 1u);
    CHECK_EQ(ticks[2], 3u);
    CHECK(saw_change);
}

LL_TEST(engine_run_pause_stop_step_modes) {
    AndLed circuit = MakeAndLed();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(circuit.doc));

    CHECK(engine.Mode() == sim::RunMode::Stopped);
    engine.Run();
    CHECK(engine.Mode() == sim::RunMode::Running);
    engine.Pause();
    CHECK(engine.Mode() == sim::RunMode::Paused);
    engine.Step();
    CHECK_EQ(engine.TickCount(), 1u);
    engine.Stop();
    CHECK(engine.Mode() == sim::RunMode::Stopped);
}
