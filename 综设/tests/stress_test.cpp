#include "test_framework.h"

#include "fixture.h"
#include "ll/sim/engine.h"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace ll;

LL_TEST(stress_independent_gates) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    constexpr int kPairs = 500;
    std::vector<ComponentId> inputs;
    std::vector<ComponentId> gates;
    inputs.reserve(kPairs);
    gates.reserve(kPairs);

    for (int i = 0; i < kPairs; ++i) {
        const int y = i * 20;
        const ComponentId in = fixture::Add(c, "input", {0, y});
        const ComponentId not_gate = fixture::Add(c, "not", {60, y});
        fixture::Connect(c, {in, 0}, {not_gate, 0});
        inputs.push_back(in);
        gates.push_back(not_gate);
    }

    const auto start = std::chrono::steady_clock::now();
    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    engine.Tick();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - start)
                             .count();

    for (int i = 0; i < kPairs; i += 97) {
        CHECK(engine.Value({gates[static_cast<std::size_t>(i)], 1}).Bit(0) ==
              LogicState::One);
    }
    CHECK(engine.Errors().empty());
    std::cout << "  [stress] " << kPairs << " gates in " << elapsed << " ms\n";
}

LL_TEST(stress_deep_combinational_chain) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    constexpr int kGates = 100;
    const ComponentId in = fixture::Add(c, "input", {0, 0});
    ComponentId previous = in;
    ComponentId last = in;
    int previous_pin = 0;

    for (int i = 0; i < kGates; ++i) {
        const ComponentId gate = fixture::Add(c, "not", {60 * (i + 1), 0});
        fixture::Connect(c, {previous, static_cast<std::uint16_t>(previous_pin)},
                         {gate, 0});
        previous = gate;
        previous_pin = 1;
        last = gate;
    }

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));

    // 100 inverters: input 0 -> output 0.
    CHECK(engine.Value({last, 1}).Bit(0) == LogicState::Zero);

    engine.SetInput({in, 0}, LogicValue::One(1));
    engine.Tick();
    CHECK(engine.Value({last, 1}).Bit(0) == LogicState::One);
    CHECK(engine.Errors().empty());
}
