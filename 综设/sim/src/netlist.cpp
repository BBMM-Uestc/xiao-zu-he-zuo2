#include "ll/sim/netlist.h"

#include "ll/component/geometry.h"
#include "ll/component/registry.h"
#include "netlist_internal.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ll::sim {
namespace {

constexpr int kSpatialCell = 64;
constexpr int kMaxHierarchyDepth = 64;

std::int64_t FloorDiv(std::int64_t a, std::int64_t b) {
    std::int64_t q = a / b;
    const std::int64_t r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) --q;
    return q;
}

struct CoordKey {
    std::uint32_t space = 0;
    int x = 0;
    int y = 0;

    bool operator<(const CoordKey& o) const {
        if (space != o.space) return space < o.space;
        if (x != o.x) return x < o.x;
        return y < o.y;
    }
};

struct CellKey {
    std::uint32_t space = 0;
    int x = 0;
    int y = 0;

    bool operator<(const CellKey& o) const {
        if (space != o.space) return space < o.space;
        if (x != o.x) return x < o.x;
        return y < o.y;
    }
};

struct Segment {
    std::uint32_t space = 0;
    GridPoint a, b;
    int node = 0;
};

struct TestPoint {
    std::uint32_t space = 0;
    GridPoint p;
    int node = 0;
    int wire_node = -1;
};

std::int64_t Cross(const GridPoint& a, const GridPoint& b, const GridPoint& c) {
    return static_cast<std::int64_t>(b.x - a.x) * (c.y - a.y) -
           static_cast<std::int64_t>(b.y - a.y) * (c.x - a.x);
}

bool PointOnSegment(const GridPoint& p, const GridPoint& a, const GridPoint& b) {
    if (Cross(a, b, p) != 0) return false;
    return p.x >= std::min(a.x, b.x) && p.x <= std::max(a.x, b.x) &&
           p.y >= std::min(a.y, b.y) && p.y <= std::max(a.y, b.y);
}

const Circuit* FindCircuitById(const Document& doc, CircuitId id) {
    for (const auto& c : doc.Circuits()) {
        if (c->Id() == id) return c.get();
    }
    return nullptr;
}

}  // namespace

class NetlistBuilder {
public:
    NetlistBuilder(const Document& doc, const ComponentRegistry& registry)
        : doc_(doc), registry_(registry) {
        parent_.push_back(-1);  // node 0 is unused
        rank_.push_back(0);
        for (const auto& c : doc_.Circuits()) {
            max_component_id_ = std::max(max_component_id_, c->MaxComponentId());
        }
        next_synthetic_id_ = max_component_id_ + 1;
    }

    BuildResult Run() {
        BuildResult result;
        std::vector<CircuitId> stack;
        stack.push_back(doc_.Main().Id());
        Status s = Expand(doc_.Main(), /*space=*/0, /*depth=*/0, stack);
        if (!s) {
            result.status = std::move(s);
            return result;
        }
        LinkTJunctions();
        result.netlist = BuildNetlistValue();
        result.components = std::move(components_);
        result.pins = std::move(pins_);
        return result;
    }

private:
    int AddNode() {
        const int id = static_cast<int>(parent_.size());
        parent_.push_back(id);
        rank_.push_back(0);
        return id;
    }

    int Find(int x) {
        while (parent_[x] != x) {
            parent_[x] = parent_[parent_[x]];
            x = parent_[x];
        }
        return x;
    }

    void Union(int a, int b) {
        a = Find(a);
        b = Find(b);
        if (a == b) return;
        if (rank_[a] < rank_[b]) std::swap(a, b);
        parent_[b] = a;
        if (rank_[a] == rank_[b]) ++rank_[a];
    }

    void AddCoordinate(std::uint32_t space, GridPoint p, int node) {
        const CoordKey key{space, p.x, p.y};
        const auto [it, inserted] = coord_node_.emplace(key, node);
        if (!inserted) Union(it->second, node);
    }

    int AddPin(PinRef pin, PinDir dir, std::uint8_t width, GridPoint world,
               std::uint32_t space, bool boundary, std::size_t component_index) {
        const int node = AddNode();
        AddCoordinate(space, world, node);

        PinInfo info;
        info.pin = pin;
        info.dir = dir;
        info.width = width == 0 ? 1 : std::min<std::uint8_t>(width, 64);
        info.boundary = boundary;
        info.component_index = component_index;
        pins_.push_back(info);
        pin_node_[PinKey(pin)] = node;

        test_points_.push_back(TestPoint{space, world, node, -1});
        return node;
    }

    void AddWire(const Wire& wire, std::uint32_t space,
                 const std::unordered_map<ComponentId, ComponentId>& id_map) {
        if (wire.points.size() < 2) return;

        const int node = AddNode();
        for (const GridPoint& p : wire.points) {
            AddCoordinate(space, p, node);
            test_points_.push_back(TestPoint{space, p, node, node});
        }

        for (std::size_t i = 0; i + 1 < wire.points.size(); ++i) {
            const GridPoint a = wire.points[i];
            const GridPoint b = wire.points[i + 1];
            if (a == b) continue;
            const int segment_index = static_cast<int>(segments_.size());
            segments_.push_back(Segment{space, a, b, node});
            IndexSegment(segment_index, space, a, b);
        }

        const auto bind = [&](const WireEndpoint& endpoint) {
            if (!endpoint.attached) return;
            const auto it = id_map.find(endpoint.pin.component);
            if (it == id_map.end()) return;
            const auto pin_it = pin_node_.find(PinKey(PinRef{it->second, endpoint.pin.pin}));
            if (pin_it != pin_node_.end()) Union(node, pin_it->second);
        };
        bind(wire.from);
        bind(wire.to);
    }

    void IndexSegment(int segment_index, std::uint32_t space, GridPoint a, GridPoint b) {
        const int min_cx = static_cast<int>(FloorDiv(std::min(a.x, b.x), kSpatialCell));
        const int max_cx = static_cast<int>(FloorDiv(std::max(a.x, b.x), kSpatialCell));
        const int min_cy = static_cast<int>(FloorDiv(std::min(a.y, b.y), kSpatialCell));
        const int max_cy = static_cast<int>(FloorDiv(std::max(a.y, b.y), kSpatialCell));
        for (int cx = min_cx; cx <= max_cx; ++cx) {
            for (int cy = min_cy; cy <= max_cy; ++cy) {
                segment_cells_[CellKey{space, cx, cy}].push_back(segment_index);
            }
        }
    }

    void LinkTJunctions() {
        for (const TestPoint& point : test_points_) {
            const CellKey cell{point.space,
                               static_cast<int>(FloorDiv(point.p.x, kSpatialCell)),
                               static_cast<int>(FloorDiv(point.p.y, kSpatialCell))};
            const auto it = segment_cells_.find(cell);
            if (it == segment_cells_.end()) continue;
            for (const int segment_index : it->second) {
                const Segment& segment = segments_[static_cast<std::size_t>(segment_index)];
                if (segment.space != point.space) continue;
                if (segment.node == point.wire_node) continue;
                if (PointOnSegment(point.p, segment.a, segment.b)) {
                    Union(point.node, segment.node);
                }
            }
        }
    }

    Status Expand(const Circuit& circuit, std::uint32_t space, int depth,
                  std::vector<CircuitId>& stack) {
        if (depth > kMaxHierarchyDepth) {
            return Status::Fail(StatusCode::CycleDetected, "子电路层次超过 64 层");
        }

        std::unordered_map<ComponentId, ComponentId> id_map;
        id_map.reserve(circuit.Components().size());

        for (const ComponentInstance& comp : circuit.Components()) {
            const ComponentDef* def = registry_.Find(comp.type);
            if (def == nullptr) {
                return Status::Fail(StatusCode::UnsupportedType,
                                    "未知元件类型: " + comp.type);
            }

            ComponentId flat_id = comp.id;
            if (depth > 0) {
                flat_id = next_synthetic_id_++;
                if (flat_id == kInvalidComponent) {
                    return Status::Fail(StatusCode::InvalidArgument,
                                        "元件 ID 空间耗尽（子电路过大）");
                }
            }
            id_map[comp.id] = flat_id;

            if (def->IsSubcircuit()) {
                const auto* sub = dynamic_cast<const SubcircuitDef*>(def);
                if (sub == nullptr) {
                    return Status::Fail(StatusCode::UnsupportedType,
                                        "子电路元件缺少 SubcircuitDef");
                }
                const Circuit* target = FindCircuitById(doc_, sub->Target());
                if (target == nullptr) {
                    return Status::Fail(StatusCode::NotFound,
                                        "子电路目标电路不存在: " + comp.type);
                }
                if (std::find(stack.begin(), stack.end(), target->Id()) != stack.end()) {
                    return Status::Fail(StatusCode::CycleDetected,
                                        "子电路存在递归引用: " + comp.type);
                }

                const auto pins = def->Pins(comp.attrs);
                const auto& ports = target->Ports();
                if (pins.size() != ports.size()) {
                    return Status::Fail(StatusCode::InvalidArgument,
                                        "子电路端口数与实例引脚数不一致: " + comp.type);
                }

                const std::uint32_t child_space = ++next_space_;
                for (std::size_t i = 0; i < pins.size(); ++i) {
                    const int boundary = AddPin(
                        PinRef{flat_id, static_cast<std::uint16_t>(i)}, pins[i].dir,
                        pins[i].width, TransformLocal(comp, pins[i].offset), space,
                        /*boundary=*/true,
                        std::numeric_limits<std::size_t>::max());

                    const int anchor = AddNode();
                    AddCoordinate(child_space, ports[i].position, anchor);
                    test_points_.push_back(
                        TestPoint{child_space, ports[i].position, anchor, -1});
                    Union(boundary, anchor);
                }

                stack.push_back(target->Id());
                Status s = Expand(*target, child_space, depth + 1, stack);
                stack.pop_back();
                if (!s) return s;
                continue;
            }

            FlatComponent flat;
            flat.id = flat_id;
            flat.def = def;
            flat.attrs = comp.attrs;
            flat.pins = def->Pins(comp.attrs);

            const std::size_t component_index = components_.size();
            components_.push_back(std::move(flat));

            for (std::size_t i = 0; i < components_.back().pins.size(); ++i) {
                const PinDef& pin = components_.back().pins[i];
                if (i > std::numeric_limits<std::uint16_t>::max()) {
                    return Status::Fail(StatusCode::InvalidArgument,
                                        "元件引脚数超过 65535: " + comp.type);
                }
                AddPin(PinRef{flat_id, static_cast<std::uint16_t>(i)}, pin.dir,
                       pin.width, TransformLocal(comp, pin.offset), space,
                       /*boundary=*/false, component_index);
            }
        }

        for (const Wire& wire : circuit.Wires()) {
            AddWire(wire, space, id_map);
        }
        return Status::Ok();
    }

    Netlist BuildNetlistValue() {
        // Group pins by union-find root.
        std::map<int, std::size_t> root_to_group;
        std::vector<std::vector<PinRef>> groups;
        for (const PinInfo& info : pins_) {
            const int node = pin_node_.at(PinKey(info.pin));
            const int root = Find(node);
            auto [it, inserted] = root_to_group.emplace(root, groups.size());
            if (inserted) groups.emplace_back();
            groups[it->second].push_back(info.pin);
        }

        for (auto& group : groups) {
            std::sort(group.begin(), group.end(),
                      [](PinRef a, PinRef b) { return PinKey(a) < PinKey(b); });
        }
        std::sort(groups.begin(), groups.end(),
                  [](const std::vector<PinRef>& a, const std::vector<PinRef>& b) {
                      return PinKey(a.front()) < PinKey(b.front());
                  });

        Netlist netlist;
        netlist.pins_by_net_.resize(groups.size() + 1);
        netlist.nets_.reserve(groups.size());

        std::uint64_t hash = 1469598103934665603ull;
        const auto mix = [&hash](std::uint64_t v) {
            for (int i = 0; i < 8; ++i) {
                hash ^= (v >> (i * 8)) & 0xffu;
                hash *= 1099511628211ull;
            }
        };
        mix(groups.size());

        for (std::size_t i = 0; i < groups.size(); ++i) {
            const NetId id{static_cast<std::uint32_t>(i + 1)};
            netlist.nets_.push_back(id);
            netlist.pins_by_net_[i + 1] = groups[i];
            mix(groups[i].size());
            for (PinRef pin : groups[i]) {
                netlist.pin_to_net_[PinKey(pin)] = id;
                mix(PinKey(pin));
            }
        }
        netlist.fingerprint_ = hash;

        return netlist;
    }

    const Document& doc_;
    const ComponentRegistry& registry_;

    std::vector<int> parent_;
    std::vector<int> rank_;
    std::map<CoordKey, int> coord_node_;
    std::map<CellKey, std::vector<int>> segment_cells_;
    std::vector<Segment> segments_;
    std::vector<TestPoint> test_points_;

    std::vector<PinInfo> pins_;
    std::vector<FlatComponent> components_;
    std::unordered_map<std::uint64_t, int> pin_node_;

    ComponentId max_component_id_ = 0;
    ComponentId next_synthetic_id_ = 1;
    std::uint32_t next_space_ = 0;
};

const std::vector<PinRef>& Netlist::PinsOf(NetId id) const {
    static const std::vector<PinRef> kEmpty;
    if (!id.valid() || id.value >= pins_by_net_.size()) return kEmpty;
    return pins_by_net_[id.value];
}

NetId Netlist::NetOf(PinRef pin) const {
    const auto it = pin_to_net_.find(PinKey(pin));
    return it == pin_to_net_.end() ? NetId{} : it->second;
}

BuildResult BuildGraph(const Document& doc, const ComponentRegistry& registry) {
    NetlistBuilder builder(doc, registry);
    return builder.Run();
}

Netlist BuildNetlist(const Document& doc) {
    return BuildGraph(doc, BuiltinComponents()).netlist;
}

Netlist BuildNetlist(const Document& doc, const ComponentRegistry& registry) {
    return BuildGraph(doc, registry).netlist;
}

}  // namespace ll::sim
