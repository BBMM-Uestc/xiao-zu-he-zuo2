// owner: C  stability: frozen
#pragma once

#include "ll/component/component_def.h"

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ll {

class ComponentRegistry {
public:
    void Register(std::unique_ptr<ComponentDef> def);
    const ComponentDef* Find(std::string_view type) const;

    std::vector<const ComponentDef*> ByCategory(std::string_view category) const;
    std::vector<std::string> Categories() const;

    std::size_t Size() const { return defs_.size(); }

private:
    std::map<std::string, std::unique_ptr<ComponentDef>, std::less<>> defs_;
    std::vector<std::string> registration_order_;
};

// Process-wide, read-only built-in registry. Tests may construct their own.
const ComponentRegistry& BuiltinComponents();

}  // namespace ll
