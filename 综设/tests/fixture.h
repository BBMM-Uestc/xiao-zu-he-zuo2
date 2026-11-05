#pragma once

#include "ll/component/geometry.h"
#include "ll/component/registry.h"
#include "ll/model/document.h"

#include <cassert>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fixture {

using namespace ll;

inline const ComponentDef& Def(std::string_view type) {
    const ComponentDef* def = BuiltinComponents().Find(type);
    assert(def != nullptr);
    return *def;
}

inline GridPoint PinPos(const Circuit& circuit, PinRef pin) {
    const ComponentInstance* inst = circuit.FindComponent(pin.component);
    assert(inst != nullptr);
    return PinWorldPos(*inst, Def(inst->type), pin.pin);
}

inline ComponentId Add(Circuit& circuit, std::string type, GridPoint pos,
                       AttrMap attrs = {}, std::string label = {}) {
    ComponentSpec spec;
    spec.type = std::move(type);
    spec.position = pos;
    spec.attrs = std::move(attrs);
    spec.label = std::move(label);
    return circuit.AddComponent(spec);
}

inline WireId Connect(Circuit& circuit, PinRef from, PinRef to,
                      std::vector<GridPoint> middle = {}) {
    std::vector<GridPoint> points;
    points.push_back(PinPos(circuit, from));
    points.insert(points.end(), middle.begin(), middle.end());
    points.push_back(PinPos(circuit, to));
    WireSpec spec;
    spec.points = std::move(points);
    spec.from.attached = true;
    spec.from.pin = from;
    spec.to.attached = true;
    spec.to.pin = to;
    return circuit.AddWire(spec);
}

inline WireId ConnectFree(Circuit& circuit, PinRef from, GridPoint to,
                          std::vector<GridPoint> middle = {}) {
    std::vector<GridPoint> points;
    points.push_back(PinPos(circuit, from));
    points.insert(points.end(), middle.begin(), middle.end());
    points.push_back(to);
    WireSpec spec;
    spec.points = std::move(points);
    spec.from.attached = true;
    spec.from.pin = from;
    spec.to.attached = false;
    spec.to.point = to;
    return circuit.AddWire(spec);
}

inline WireId FreeWire(Circuit& circuit, GridPoint from, GridPoint to) {
    WireSpec spec;
    spec.points = {from, to};
    spec.from.attached = false;
    spec.from.point = from;
    spec.to.attached = false;
    spec.to.point = to;
    return circuit.AddWire(spec);
}

}  // namespace fixture
