#include "ll/component/registry.h"

#include <algorithm>
#include <cassert>

namespace ll {

void ComponentRegistry::Register(std::unique_ptr<ComponentDef> def) {
    assert(def != nullptr);
    const std::string type(def->Type());
    assert(defs_.find(type) == defs_.end() && "duplicate component type");
    registration_order_.push_back(type);
    defs_.emplace(type, std::move(def));
}

const ComponentDef* ComponentRegistry::Find(std::string_view type) const {
    auto it = defs_.find(type);
    return it == defs_.end() ? nullptr : it->second.get();
}

std::vector<const ComponentDef*> ComponentRegistry::ByCategory(
    std::string_view category) const {
    std::vector<const ComponentDef*> out;
    for (const std::string& type : registration_order_) {
        const auto it = defs_.find(type);
        if (it != defs_.end() && it->second->Category() == category) {
            out.push_back(it->second.get());
        }
    }
    return out;
}

std::vector<std::string> ComponentRegistry::Categories() const {
    std::vector<std::string> out;
    for (const std::string& type : registration_order_) {
        const auto it = defs_.find(type);
        if (it == defs_.end()) continue;
        const std::string_view category = it->second->Category();
        if (std::find(out.begin(), out.end(), category) == out.end()) {
            out.emplace_back(category);
        }
    }
    return out;
}

}  // namespace ll
