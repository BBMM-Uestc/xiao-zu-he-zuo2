#pragma once

#include <string>
#include <vector>

namespace logicsim {

struct ComponentDescriptor {
    std::string type;
    std::string displayName;
};

class ComponentCatalog {
public:
    [[nodiscard]] static const std::vector<ComponentDescriptor>& BuiltIns();
};

}  // namespace logicsim
