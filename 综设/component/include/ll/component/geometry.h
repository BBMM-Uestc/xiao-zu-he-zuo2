// owner: C  stability: frozen
#pragma once

#include "ll/component/component_def.h"
#include "ll/model/circuit.h"

namespace ll {

// Transform a local pin offset by mirror-then-rotate and add the component
// origin. Both editor and simulator must use this function.
GridPoint PinWorldPos(const ComponentInstance&, const ComponentDef&,
                      std::uint16_t pin_index);

// Bounding rectangle in world coordinates, derived from the pure-data symbol
// and the pin positions. Used for culling and hit testing.
GridRect ComponentBounds(const ComponentInstance&, const ComponentDef&,
                         const AttrMap&);

// Apply the local transform (mirror first, then rotate) to one point.
GridPoint TransformLocal(const ComponentInstance&, GridPoint local);

}  // namespace ll
