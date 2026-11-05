#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace logicsim {

using ComponentId = std::uint64_t;
using WireId = std::uint64_t;

enum class LogicValue {
    Zero,
    One,
    Unknown,
    HighZ,
    Error
};

enum class PinDirection {
    Input,
    Output
};

struct PinDefinition {
    std::string id;
    PinDirection direction{PinDirection::Input};
};

struct Point {
    double x{};
    double y{};
};

struct ComponentInstance {
    ComponentId id{};
    std::string type;
    double x{};
    double y{};
    int rotation{};
    std::unordered_map<std::string, std::string> properties;
};

struct Wire {
    WireId id{};
    ComponentId fromComponent{};
    std::string fromPin;
    ComponentId toComponent{};
    std::string toPin;
    std::vector<Point> route;
};

struct CircuitDocument {
    static constexpr int CurrentVersion = 1;

    int version{CurrentVersion};
    std::string name{"Untitled"};
    std::vector<ComponentInstance> components;
    std::vector<Wire> wires;
};

[[nodiscard]] ComponentId NextComponentId(const CircuitDocument& document);
[[nodiscard]] WireId NextWireId(const CircuitDocument& document);

}  // namespace logicsim
