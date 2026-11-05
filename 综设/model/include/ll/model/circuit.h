// owner: D  stability: frozen
#pragma once

#include "ll/model/types.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ll {

struct PinRef {
    ComponentId component = kInvalidComponent;
    std::uint16_t pin = 0;

    bool valid() const { return component != kInvalidComponent; }
    bool operator==(const PinRef&) const = default;
};

constexpr std::uint64_t PinKey(PinRef p) {
    return (static_cast<std::uint64_t>(p.component) << 32) |
           static_cast<std::uint64_t>(p.pin);
}

struct ComponentInstance {
    ComponentId id = kInvalidComponent;
    std::string type;          // ComponentDef::Type()
    GridPoint   position;      // component origin, grid units
    int         rotation = 0;  // 0/90/180/270, counter-clockwise on screen
    bool        mirrored = false;
    AttrMap     attrs;
    std::string label;
};

struct WireEndpoint {
    bool     attached = false;  // true: endpoint binds to a pin
    PinRef   pin;               // valid when attached
    GridPoint point;            // free coordinate when not attached
};

struct Wire {
    WireId  id = 0;
    std::vector<GridPoint> points;  // polyline vertices, at least 2
    WireEndpoint from, to;
};

struct Port {  // subcircuit boundary pin
    std::string name;
    PinDir      dir = PinDir::Input;
    std::uint8_t width = 1;
    GridPoint   position;
};

struct ComponentSpec {
    std::string type;
    GridPoint   position;
    int         rotation = 0;
    bool        mirrored = false;
    AttrMap     attrs;
    std::string label;
};

struct WireSpec {
    std::vector<GridPoint> points;
    WireEndpoint from, to;
};

// The model increments Document::Revision() and Dirty() through this sink.
struct ChangeSink {
    std::uint64_t* revision = nullptr;
    bool*          dirty = nullptr;
};

class Document;

class Circuit {
public:
    Circuit(CircuitId id, std::string name, ChangeSink sink);

    Circuit(const Circuit&) = delete;
    Circuit& operator=(const Circuit&) = delete;

    CircuitId Id() const { return id_; }
    std::string_view Name() const { return name_; }

    const std::vector<ComponentInstance>& Components() const { return components_; }
    const std::vector<Wire>& Wires() const { return wires_; }
    const std::vector<Port>& Ports() const { return ports_; }

    const ComponentInstance* FindComponent(ComponentId) const;
    const Wire*              FindWire(WireId) const;

    // ---- normal editing: allocates a fresh id ----
    ComponentId AddComponent(const ComponentSpec&);
    WireId      AddWire(const WireSpec&);
    bool        RemoveComponent(ComponentId);  // detaches wires bound to it
    bool        RemoveWire(WireId);
    bool        SetAttr(ComponentId, std::string key, AttrValue);
    bool        SetPosition(ComponentId, GridPoint);
    bool        SetTransform(ComponentId, int rotation, bool mirrored);

    Port&       AddPort(Port);                 // additive helper (needed by tests)
    bool        RemovePort(std::string_view name);
    bool        SetPorts(std::vector<Port>);

    // ---- undo/redo: keep the original id ----
    void RestoreComponent(ComponentInstance);  // asserts if the id already exists
    void RestoreWire(Wire);
    void RestorePort(Port);

    ComponentId MaxComponentId() const { return max_component_id_; }
    WireId      MaxWireId() const { return max_wire_id_; }

    Status Validate() const;

private:
    friend class Document;

    void Bump();
    static void NormalizePoints(std::vector<GridPoint>& points);

    CircuitId id_;
    std::string name_;
    ChangeSink sink_;

    std::vector<ComponentInstance> components_;
    std::vector<Wire> wires_;
    std::vector<Port> ports_;

    std::uint32_t next_component_id_ = 1;
    std::uint32_t next_wire_id_ = 1;
    ComponentId max_component_id_ = 0;
    WireId max_wire_id_ = 0;
};

}  // namespace ll
