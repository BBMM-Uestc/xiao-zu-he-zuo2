#include "SimulationEngine.h"

#include <utility>

namespace logicsim {

void SimulationEngine::Reset(CircuitDocument document) {
    document_ = std::move(document);
    snapshot_ = {};

    for (const auto& component : document_.components) {
        snapshot_.componentValues.emplace(component.id, LogicValue::Unknown);
    }
}

void SimulationEngine::Step() {
    // The first integrated milestone adds net compilation and value propagation here.
}

void SimulationEngine::SetRunning(bool running) {
    snapshot_.running = running;
}

void SimulationEngine::SetComponentValue(ComponentId componentId, LogicValue value) {
    snapshot_.componentValues[componentId] = value;
}

bool SimulationEngine::IsRunning() const noexcept {
    return snapshot_.running;
}

const CircuitDocument& SimulationEngine::GetDocument() const noexcept {
    return document_;
}

const SimulationSnapshot& SimulationEngine::GetSnapshot() const noexcept {
    return snapshot_;
}

}  // namespace logicsim
