#include "ll/component/geometry.h"

#include <algorithm>
#include <cassert>
#include <limits>

namespace ll {
namespace {

GridPoint Rotate(GridPoint p, int rotation) {
    switch (rotation) {
        case 0:   return p;
        case 90:  return {p.y, -p.x};
        case 180: return {-p.x, -p.y};
        case 270: return {-p.y, p.x};
        default:  assert(false && "invalid rotation"); return p;
    }
}

struct BoundsAccumulator {
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = std::numeric_limits<int>::min();
    int max_y = std::numeric_limits<int>::min();

    void Add(GridPoint p) {
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
        max_x = std::max(max_x, p.x);
        max_y = std::max(max_y, p.y);
    }

    GridRect Get() const {
        if (min_x > max_x) return {};
        return {{min_x, min_y}, {max_x, max_y}};
    }
};

void AddRect(BoundsAccumulator& acc, const ComponentInstance& inst, GridRect r) {
    acc.Add(TransformLocal(inst, r.tl));
    acc.Add(TransformLocal(inst, {r.br.x, r.tl.y}));
    acc.Add(TransformLocal(inst, {r.tl.x, r.br.y}));
    acc.Add(TransformLocal(inst, r.br));
}

}  // namespace

GridPoint TransformLocal(const ComponentInstance& inst, GridPoint local) {
    if (inst.mirrored) local.x = -local.x;
    const GridPoint rotated = Rotate(local, inst.rotation);
    return inst.position + rotated;
}

GridPoint PinWorldPos(const ComponentInstance& inst, const ComponentDef& def,
                      std::uint16_t pin_index) {
    const auto pins = def.Pins(inst.attrs);
    if (pin_index >= pins.size()) {
        assert(false && "PinWorldPos: pin index out of range");
        return inst.position;
    }
    return TransformLocal(inst, pins[pin_index].offset);
}

GridRect ComponentBounds(const ComponentInstance& inst, const ComponentDef& def,
                         const AttrMap& attrs) {
    BoundsAccumulator acc;

    const DrawList symbol = def.BaseSymbol(attrs);
    for (const Shape& shape : symbol.shapes) {
        switch (shape.kind) {
            case Shape::Kind::Line:
            case Shape::Kind::Polyline:
                for (const GridPoint& p : shape.points) {
                    acc.Add(TransformLocal(inst, p));
                }
                break;
            case Shape::Kind::Rect:
            case Shape::Kind::Circle:
            case Shape::Kind::Arc:
                AddRect(acc, inst, shape.rect);
                break;
            case Shape::Kind::Text: {
                const int w = static_cast<int>(shape.text.size()) * shape.font_size / 2;
                const int h = shape.font_size;
                AddRect(acc, inst, {shape.rect.tl, {shape.rect.tl.x + w, shape.rect.tl.y + h}});
                break;
            }
        }
    }

    const auto pins = def.Pins(attrs);
    for (std::uint16_t i = 0; i < pins.size(); ++i) {
        acc.Add(PinWorldPos(inst, def, i));
    }

    return acc.Get();
}

}  // namespace ll
