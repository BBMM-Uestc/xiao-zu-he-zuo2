#include "ll/model/document.h"

#include <algorithm>
#include <cassert>

namespace ll {

Document::Document() : Document(std::string("main")) {}

Document::Document(std::string main_circuit) : main_name_(std::move(main_circuit)) {
    if (main_name_.empty()) main_name_ = "main";
    AddCircuit(main_name_);
}

Document::~Document() = default;

Document::Document(Document&& other) noexcept
    : main_name_(std::move(other.main_name_)),
      circuits_(std::move(other.circuits_)),
      revision_(other.revision_),
      dirty_(other.dirty_),
      next_circuit_id_(other.next_circuit_id_) {
    other.revision_ = 0;
    other.dirty_ = false;
    other.next_circuit_id_ = 1;
    other.main_name_ = "main";
    RebindCircuits();
}

Document& Document::operator=(Document&& other) noexcept {
    if (this == &other) return *this;
    main_name_ = std::move(other.main_name_);
    circuits_ = std::move(other.circuits_);
    revision_ = other.revision_;
    dirty_ = other.dirty_;
    next_circuit_id_ = other.next_circuit_id_;

    other.revision_ = 0;
    other.dirty_ = false;
    other.next_circuit_id_ = 1;
    other.main_name_ = "main";
    RebindCircuits();
    return *this;
}

Document Document::CreateEmpty(std::string main_circuit) {
    return Document(std::move(main_circuit));
}

Circuit& Document::AddCircuit(std::string name) {
    const CircuitId id = next_circuit_id_++;
    circuits_.push_back(
        std::make_unique<Circuit>(id, std::move(name), ChangeSink{&revision_, &dirty_}));
    return *circuits_.back();
}

void Document::RebindCircuits() {
    for (auto& c : circuits_) c->sink_ = ChangeSink{&revision_, &dirty_};
}

Circuit& Document::Main() {
    for (auto& c : circuits_) {
        if (c->Name() == main_name_) return *c;
    }
    // A Document always owns its main circuit; repair a broken invariant.
    assert(false && "main circuit missing");
    return AddCircuit(main_name_);
}

const Circuit& Document::Main() const {
    for (const auto& c : circuits_) {
        if (c->Name() == main_name_) return *c;
    }
    assert(false && "main circuit missing");
    return *circuits_.front();
}

Circuit* Document::FindCircuit(std::string_view name) {
    for (auto& c : circuits_) {
        if (c->Name() == name) return c.get();
    }
    return nullptr;
}

const Circuit* Document::FindCircuit(std::string_view name) const {
    for (const auto& c : circuits_) {
        if (c->Name() == name) return c.get();
    }
    return nullptr;
}

Circuit& Document::CreateCircuit(std::string_view name) {
    assert(!name.empty());
    if (Circuit* existing = FindCircuit(name)) return *existing;
    Circuit& c = AddCircuit(std::string(name));
    ++revision_;
    dirty_ = true;
    return c;
}

bool Document::RemoveCircuit(std::string_view name) {
    if (name == main_name_) return false;
    auto it = std::find_if(circuits_.begin(), circuits_.end(),
                           [&](const std::unique_ptr<Circuit>& c) {
                               return c->Name() == name;
                           });
    if (it == circuits_.end()) return false;
    circuits_.erase(it);
    ++revision_;
    dirty_ = true;
    return true;
}

Status Document::Validate() const {
    if (circuits_.empty()) {
        return Status::Fail(StatusCode::InvalidArgument, "文档没有电路");
    }

    bool has_main = false;
    std::vector<std::string_view> names;
    for (const auto& c : circuits_) {
        if (c->Name() == main_name_) has_main = true;
        if (std::find(names.begin(), names.end(), c->Name()) != names.end()) {
            return Status::Fail(StatusCode::DuplicateName, "电路名重复");
        }
        names.push_back(c->Name());
        if (Status s = c->Validate(); !s) return s;
    }

    if (!has_main) {
        return Status::Fail(StatusCode::NotFound, "找不到主电路");
    }
    return Status::Ok();
}

}  // namespace ll
