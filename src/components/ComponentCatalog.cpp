#include "ComponentCatalog.h"

namespace logicsim {

const std::vector<ComponentDescriptor>& ComponentCatalog::BuiltIns() {
    static const std::vector<ComponentDescriptor> components{
        {"input", "Input"},
        {"output", "Output"},
        {"led", "LED"},
        {"clock", "Clock"},
        {"and", "AND Gate"},
        {"or", "OR Gate"},
        {"not", "NOT Gate"},
    };
    return components;
}

}  // namespace logicsim
