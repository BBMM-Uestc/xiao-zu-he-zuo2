#include "test_framework.h"

#include "fixture.h"
#include "ll/sim/engine.h"

using namespace ll;

LL_TEST(waveform_records_changes_on_watched_net) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId in = fixture::Add(c, "input", {0, 0});
    const ComponentId led = fixture::Add(c, "led", {70, 0});
    fixture::Connect(c, {in, 0}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));

    const sim::NetId net = engine.NetlistRef().NetOf({led, 0});
    REQUIRE(net.valid());

    engine.SetRecording(true);
    engine.GetWaveform().SetWatched({net});

    engine.SetInput({in, 0}, LogicValue::One(1));
    engine.Tick();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 1u);
    CHECK(engine.GetWaveform().Samples()[0].value.Bit(0) == LogicState::One);
    CHECK_EQ(engine.GetWaveform().Samples()[0].tick, 1u);

    engine.Tick();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 1u);

    engine.SetInput({in, 0}, LogicValue::Zero(1));
    engine.Tick();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 2u);
    CHECK(engine.GetWaveform().Samples()[1].value.Bit(0) == LogicState::Zero);
}

LL_TEST(waveform_clear_keeps_watch_list) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();
    const ComponentId in = fixture::Add(c, "input", {0, 0});
    const ComponentId led = fixture::Add(c, "led", {70, 0});
    fixture::Connect(c, {in, 0}, {led, 0});

    sim::Engine engine(BuiltinComponents());
    REQUIRE(engine.Load(doc));
    const sim::NetId net = engine.NetlistRef().NetOf({led, 0});
    engine.SetRecording(true);
    engine.GetWaveform().SetWatched({net});

    engine.SetInput({in, 0}, LogicValue::One(1));
    engine.Tick();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 1u);

    engine.GetWaveform().Clear();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 0u);
    CHECK_EQ(engine.GetWaveform().WatchedNets().size(), 1u);

    engine.SetInput({in, 0}, LogicValue::Zero(1));
    engine.Tick();
    CHECK_EQ(engine.GetWaveform().SampleCount(), 1u);
}
