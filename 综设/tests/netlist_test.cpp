#include "test_framework.h"

#include "fixture.h"
#include "ll/sim/netlist.h"

using namespace ll;

LL_TEST(netlist_and_led_has_three_nets) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId b = fixture::Add(c, "input", {0, 20});
    const ComponentId g = fixture::Add(c, "and", {40, 20});
    const ComponentId led = fixture::Add(c, "led", {90, 20});

    fixture::Connect(c, {a, 0}, {g, 0});
    fixture::Connect(c, {b, 0}, {g, 1});
    fixture::Connect(c, {g, 2}, {led, 0});

    const sim::Netlist netlist = sim::BuildNetlist(doc);
    CHECK_EQ(netlist.NetCount(), 3u);
    CHECK_EQ(netlist.Nets().size(), 3u);

    CHECK(netlist.NetOf({a, 0}) == netlist.NetOf({g, 0}));
    CHECK(netlist.NetOf({b, 0}) == netlist.NetOf({g, 1}));
    CHECK(netlist.NetOf({g, 2}) == netlist.NetOf({led, 0}));
    CHECK(!(netlist.NetOf({a, 0}) == netlist.NetOf({b, 0})));

    CHECK_EQ(netlist.PinsOf(netlist.NetOf({a, 0})).size(), 2u);
}

LL_TEST(netlist_t_junction_connects) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId b = fixture::Add(c, "input", {0, 30});
    const ComponentId o = fixture::Add(c, "output", {100, 0});

    fixture::Connect(c, {a, 0}, {o, 0});
    const GridPoint tap{50, 0};
    fixture::ConnectFree(c, {b, 0}, tap);

    const sim::Netlist netlist = sim::BuildNetlist(doc);
    CHECK(netlist.NetOf({a, 0}) == netlist.NetOf({b, 0}));
    CHECK(netlist.NetOf({a, 0}) == netlist.NetOf({o, 0}));
}

LL_TEST(netlist_crossing_without_endpoint_does_not_connect) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId o = fixture::Add(c, "output", {100, 0});
    const ComponentId b = fixture::Add(c, "input", {40, 30});
    const ComponentId o2 = fixture::Add(c, "output", {80, -30});

    fixture::Connect(c, {a, 0}, {o, 0});
    fixture::Connect(c, {b, 0}, {o2, 0});

    const sim::Netlist netlist = sim::BuildNetlist(doc);
    CHECK(netlist.NetOf({a, 0}) == netlist.NetOf({o, 0}));
    CHECK(netlist.NetOf({b, 0}) == netlist.NetOf({o2, 0}));
    CHECK(!(netlist.NetOf({a, 0}) == netlist.NetOf({b, 0})));
}

LL_TEST(netlist_fingerprint_is_stable_and_sensitive_to_structure) {
    Document doc = Document::CreateEmpty();
    Circuit& c = doc.Main();

    const ComponentId a = fixture::Add(c, "input", {0, 0});
    const ComponentId g = fixture::Add(c, "not", {40, 0});
    const ComponentId o = fixture::Add(c, "output", {90, 0});
    fixture::Connect(c, {a, 0}, {g, 0});
    fixture::Connect(c, {g, 1}, {o, 0});

    const auto first = sim::BuildNetlist(doc).Fingerprint();
    const auto second = sim::BuildNetlist(doc).Fingerprint();
    CHECK_EQ(first, second);

    c.RemoveComponent(o);
    const auto third = sim::BuildNetlist(doc).Fingerprint();
    CHECK(first != third);
}
