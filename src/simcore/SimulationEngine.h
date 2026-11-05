#pragma once

#include "CircuitDocument.h"

#include <unordered_map>

namespace logicsim {

struct SimulationSnapshot {
    bool running{};
    std::unordered_map<ComponentId, LogicValue> componentValues;
};

class SimulationEngine {
public:
    void Reset(CircuitDocument document);
    void Step();
    void SetRunning(bool running);
    void SetComponentValue(ComponentId componentId, LogicValue value);

    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] const CircuitDocument& GetDocument() const noexcept;
    [[nodiscard]] const SimulationSnapshot& GetSnapshot() const noexcept;

private:
    CircuitDocument document_;
    SimulationSnapshot snapshot_;
};

}  // namespace logicsim
