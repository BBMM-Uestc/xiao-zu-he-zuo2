#include "CircuitDocument.h"

#include <algorithm>

namespace logicsim {

ComponentId NextComponentId(const CircuitDocument& document) {
    ComponentId nextId = 1;
    for (const auto& component : document.components) {
        nextId = std::max(nextId, component.id + 1);
    }
    return nextId;
}

WireId NextWireId(const CircuitDocument& document) {
    WireId nextId = 1;
    for (const auto& wire : document.wires) {
        nextId = std::max(nextId, wire.id + 1);
    }
    return nextId;
}

}  // namespace logicsim
