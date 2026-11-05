#include "ll/model/circuit.h"

#include <algorithm>
#include <cassert>
#include <unordered_set>

namespace ll {
namespace {

bool IsValidRotation(int r) {
    return r == 0 || r == 90 || r == 180 || r == 270;
}

long long Cross(const GridPoint& a, const GridPoint& b, const GridPoint& c) {
    return static_cast<long long>(b.x - a.x) * (c.y - b.y) -
           static_cast<long long>(b.y - a.y) * (c.x - b.x);
}

}  // namespace

Circuit::Circuit(CircuitId id, std::string name, ChangeSink sink)
    : id_(id), name_(std::move(name)), sink_(sink) {
    assert(id_ != kInvalidCircuit);
}

const ComponentInstance* Circuit::FindComponent(ComponentId id) const {
    for (const auto& c : components_) {
        if (c.id == id) return &c;
    }
    return nullptr;
}

const Wire* Circuit::FindWire(WireId id) const {
    for (const auto& w : wires_) {
        if (w.id == id) return &w;
    }
    return nullptr;
}

void Circuit::Bump() {
    if (sink_.revision) ++*sink_.revision;
    if (sink_.dirty) *sink_.dirty = true;
}

ComponentId Circuit::AddComponent(const ComponentSpec& spec) {
    assert(IsValidRotation(spec.rotation));
    ComponentInstance c;
    c.id = next_component_id_++;
    c.type = spec.type;
    c.position = spec.position;
    c.rotation = spec.rotation;
    c.mirrored = spec.mirrored;
    c.attrs = spec.attrs;
    c.label = spec.label;
    components_.push_back(std::move(c));
    max_component_id_ = std::max(max_component_id_, components_.back().id);
    Bump();
    return components_.back().id;
}

WireId Circuit::AddWire(const WireSpec& spec) {
    assert(spec.points.size() >= 2);
    Wire w;
    w.id = next_wire_id_++;
    w.points = spec.points;
    NormalizePoints(w.points);
    assert(w.points.size() >= 2);
    w.from = spec.from;
    w.to = spec.to;
    wires_.push_back(std::move(w));
    max_wire_id_ = std::max(max_wire_id_, wires_.back().id);
    Bump();
    return wires_.back().id;
}

bool Circuit::RemoveComponent(ComponentId id) {
    auto it = std::find_if(components_.begin(), components_.end(),
                           [id](const ComponentInstance& c) { return c.id == id; });
    if (it == components_.end()) return false;

    for (auto& w : wires_) {
        if (w.from.attached && w.from.pin.component == id) {
            w.from.attached = false;
            w.from.pin = {};
            w.from.point = w.points.empty() ? GridPoint{} : w.points.front();
        }
        if (w.to.attached && w.to.pin.component == id) {
            w.to.attached = false;
            w.to.pin = {};
            w.to.point = w.points.empty() ? GridPoint{} : w.points.back();
        }
    }

    components_.erase(it);
    Bump();
    return true;
}

bool Circuit::RemoveWire(WireId id) {
    auto it = std::find_if(wires_.begin(), wires_.end(),
                           [id](const Wire& w) { return w.id == id; });
    if (it == wires_.end()) return false;
    wires_.erase(it);
    Bump();
    return true;
}

bool Circuit::SetAttr(ComponentId id, std::string key, AttrValue value) {
    for (auto& c : components_) {
        if (c.id == id) {
            c.attrs[std::move(key)] = std::move(value);
            Bump();
            return true;
        }
    }
    return false;
}

bool Circuit::SetPosition(ComponentId id, GridPoint position) {
    for (auto& c : components_) {
        if (c.id == id) {
            if (c.position == position) return true;
            c.position = position;
            Bump();
            return true;
        }
    }
    return false;
}

bool Circuit::SetTransform(ComponentId id, int rotation, bool mirrored) {
    assert(IsValidRotation(rotation));
    for (auto& c : components_) {
        if (c.id == id) {
            if (c.rotation == rotation && c.mirrored == mirrored) return true;
            c.rotation = rotation;
            c.mirrored = mirrored;
            Bump();
            return true;
        }
    }
    return false;
}

Port& Circuit::AddPort(Port port) {
    assert(!port.name.empty());
    assert(std::none_of(ports_.begin(), ports_.end(), [&](const Port& p) {
        return p.name == port.name;
    }));
    ports_.push_back(std::move(port));
    Bump();
    return ports_.back();
}

bool Circuit::RemovePort(std::string_view name) {
    auto it = std::find_if(ports_.begin(), ports_.end(),
                           [&](const Port& p) { return p.name == name; });
    if (it == ports_.end()) return false;
    ports_.erase(it);
    Bump();
    return true;
}

bool Circuit::SetPorts(std::vector<Port> ports) {
    std::unordered_set<std::string> names;
    for (const auto& p : ports) {
        assert(!p.name.empty());
        assert(names.insert(p.name).second);
    }
    ports_ = std::move(ports);
    Bump();
    return true;
}

void Circuit::RestoreComponent(ComponentInstance c) {
    assert(c.id != kInvalidComponent);
    assert(FindComponent(c.id) == nullptr);
    assert(IsValidRotation(c.rotation));
    max_component_id_ = std::max(max_component_id_, c.id);
    next_component_id_ = std::max(next_component_id_, c.id + 1);
    components_.push_back(std::move(c));
    Bump();
}

void Circuit::RestoreWire(Wire w) {
    assert(w.id != kInvalidWire);
    assert(FindWire(w.id) == nullptr);
    NormalizePoints(w.points);
    assert(w.points.size() >= 2);
    max_wire_id_ = std::max(max_wire_id_, w.id);
    next_wire_id_ = std::max(next_wire_id_, w.id + 1);
    wires_.push_back(std::move(w));
    Bump();
}

void Circuit::RestorePort(Port p) {
    assert(!p.name.empty());
    assert(std::none_of(ports_.begin(), ports_.end(), [&](const Port& q) {
        return q.name == p.name;
    }));
    ports_.push_back(std::move(p));
    Bump();
}

Status Circuit::Validate() const {
    std::unordered_set<ComponentId> component_ids;
    for (const auto& c : components_) {
        if (c.id == kInvalidComponent) {
            return Status::Fail(StatusCode::InvalidArgument, "元件 ID 不能为 0");
        }
        if (!component_ids.insert(c.id).second) {
            return Status::Fail(StatusCode::InvalidArgument, "元件 ID 重复");
        }
        if (!IsValidRotation(c.rotation)) {
            return Status::Fail(StatusCode::InvalidArgument, "元件旋转角度非法");
        }
        if (c.type.empty()) {
            return Status::Fail(StatusCode::InvalidArgument, "元件类型为空");
        }
    }

    std::unordered_set<WireId> wire_ids;
    for (const auto& w : wires_) {
        if (w.id == kInvalidWire || !wire_ids.insert(w.id).second) {
            return Status::Fail(StatusCode::InvalidArgument, "导线 ID 重复或为 0");
        }
        if (w.points.size() < 2) {
            return Status::Fail(StatusCode::InvalidArgument, "导线至少需要两个端点");
        }
        for (std::size_t i = 1; i < w.points.size(); ++i) {
            if (w.points[i] == w.points[i - 1]) {
                return Status::Fail(StatusCode::InvalidArgument, "导线存在重复顶点");
            }
        }
        if (w.from.attached && !FindComponent(w.from.pin.component)) {
            return Status::Fail(StatusCode::InvalidArgument, "导线起点引用的元件不存在");
        }
        if (w.to.attached && !FindComponent(w.to.pin.component)) {
            return Status::Fail(StatusCode::InvalidArgument, "导线终点引用的元件不存在");
        }
    }

    std::unordered_set<std::string> port_names;
    for (const auto& p : ports_) {
        if (p.name.empty() || !port_names.insert(p.name).second) {
            return Status::Fail(StatusCode::InvalidArgument, "端口名为空或重复");
        }
    }

    return Status::Ok();
}

void Circuit::NormalizePoints(std::vector<GridPoint>& points) {
    // Drop consecutive duplicate vertices.
    points.erase(std::unique(points.begin(), points.end()), points.end());

    // Drop collinear middle vertices.
    bool changed = true;
    while (changed && points.size() >= 3) {
        changed = false;
        for (std::size_t i = 1; i + 1 < points.size(); ++i) {
            if (Cross(points[i - 1], points[i], points[i + 1]) == 0) {
                points.erase(points.begin() + static_cast<std::ptrdiff_t>(i));
                changed = true;
                break;
            }
        }
    }
}

}  // namespace ll
