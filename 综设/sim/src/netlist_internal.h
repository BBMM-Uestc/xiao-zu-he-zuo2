// Internal to ll_sim. Not installed / not part of the public contract.
#pragma once

#include "ll/component/component_def.h"
#include "ll/model/document.h"
#include "ll/sim/netlist.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace ll::sim {

struct FlatComponent {
    ComponentId id = kInvalidComponent;
    const ComponentDef* def = nullptr;
    AttrMap attrs;
    std::vector<PinDef> pins;
};

struct PinInfo {
    PinRef pin;
    PinDir dir = PinDir::Input;
    std::uint8_t width = 1;
    bool boundary = false;  // subcircuit outer pin, never evaluated
    std::size_t component_index = std::numeric_limits<std::size_t>::max();
};

struct BuildResult {
    Status status = Status::Ok();
    Netlist netlist;
    std::vector<FlatComponent> components;
    std::vector<PinInfo> pins;
};

// Flattens hierarchy, computes connectivity and returns the flat component
// descriptors that Engine needs for evaluation.
BuildResult BuildGraph(const Document&, const ComponentRegistry&);

}  // namespace ll::sim
