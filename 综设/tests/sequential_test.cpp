#include "test_framework.h"

#include "fixture.h"
#include "ll/sim/engine.h"

using namespace ll;

LL_TEST(clock_levels_and_rising_edges) {
    Document doc = Document::CreateEmpty();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));

    CHECK(!engine.ClockLevel());
    engine.Tick();
    CHECK(engine.ClockLevel());
    engine.Tick();
    CHECK(!engine.ClockLevel());
    engine.Tick();
    CHECK(engine.ClockLevel());
    CHECK_EQ(engine.TickCount(), 3u);
}

LL_TEST(dff_toggles_every_two_ticks) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId dff = fixture::Add(c, "dff", {0, 0});
    fixture::Connect(c, {dff, 2}, {dff, 0});  // Q' -> D

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK(engine.Value({dff, 1}).Bit(0) == LogicState::Zero);

    engine.Tick();  // rising edge 1
    CHECK(engine.Value({dff, 1}).Bit(0) == LogicState::One);
    engine.Tick();  // falling edge
    CHECK(engine.Value({dff, 1}).Bit(0) == LogicState::One);
    engine.Tick();  // rising edge 3
    CHECK(engine.Value({dff, 1}).Bit(0) == LogicState::Zero);
    engine.Tick();
    CHECK(engine.Value({dff, 1}).Bit(0) == LogicState::Zero);

    CHECK(engine.Errors().empty());
}

LL_TEST(counter_mod_10_wraps) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId counter = fixture::Add(
        c, "counter", {0, 0},
        {{"bits", std::int64_t{4}}, {"modulus", std::int64_t{10}}});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK_EQ(engine.Value({counter, 0}).value, 0u);

    engine.Tick();
    engine.Tick();
    CHECK_EQ(engine.Value({counter, 0}).value, 1u);

    for (int i = 0; i < 18; ++i) engine.Tick();
    CHECK_EQ(engine.Value({counter, 0}).value, 0u);
    CHECK_EQ(engine.TickCount(), 20u);
}

LL_TEST(register_samples_on_rising_edge_only) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId in = fixture::Add(c, "input", {0, 0});
    const ComponentId reg = fixture::Add(c, "register", {50, 0});
    fixture::Connect(c, {in, 0}, {reg, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));

    engine.SetInput({in, 0}, LogicValue::One(1));
    engine.Tick();  // rising
    CHECK(engine.Value({reg, 1}).Bit(0) == LogicState::One);

    engine.SetInput({in, 0}, LogicValue::Zero(1));
    engine.Tick();  // falling: old value retained
    CHECK(engine.Value({reg, 1}).Bit(0) == LogicState::One);

    engine.Tick();  // rising: new value sampled
    CHECK(engine.Value({reg, 1}).Bit(0) == LogicState::Zero);
}

LL_TEST(clock_component_follows_global_clock) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId clock = fixture::Add(c, "clock", {0, 0});
    const ComponentId led = fixture::Add(c, "led", {60, 0});
    fixture::Connect(c, {clock, 0}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Zero);

    engine.Tick();
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::One);
    engine.Tick();
    CHECK(engine.Value({led, 0}).Bit(0) == LogicState::Zero);
}
