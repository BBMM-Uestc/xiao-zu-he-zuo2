// owner: C  stability: frozen
#pragma once

#include "ll/component/defs.h"
#include "ll/model/circuit.h"
#include "ll/model/logic_value.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ll {

struct EvalContext {
    // Same order and length as Pins(). Output pins carry the current net value
    // (useful for tri-state and feedback devices).
    std::span<const LogicValue> pins;
    const AttrMap& attrs;
    std::uint64_t tick = 0;
    bool clock_level = false;
    bool clock_rising = false;
};

struct EvalResult {
    // Same order and length as Pins(); only Output/InOut positions are read.
    std::span<LogicValue> pins;
};

class ComponentState {
public:
    virtual ~ComponentState() = default;
};

class ComponentDef {
public:
    virtual ~ComponentDef() = default;

    // ---- metadata ----
    virtual std::string_view Type() const = 0;
    virtual std::string_view Category() const = 0;
    virtual std::string_view DisplayName() const = 0;
    virtual std::vector<AttrDef> Attrs() const { return {}; }
    virtual std::vector<PinDef> Pins(const AttrMap&) const = 0;

    // ---- behaviour ----
    virtual void Eval(const EvalContext&, ComponentState*, EvalResult&) const = 0;

    // ---- sequential hooks (combinational components use defaults) ----
    virtual bool IsSequential() const { return false; }
    virtual std::unique_ptr<ComponentState> CreateState() const { return nullptr; }
    virtual void Reset(ComponentState*) const {}
    virtual void CommitEdge(const EvalContext&, ComponentState*) const {}

    // ---- special categories ----
    virtual bool IsClockSource() const { return false; }
    virtual bool IsSubcircuit() const { return false; }
    // Additive hook (v1.0 proposal): marks a user-driven source such as the
    // "input" or "button" component. The engine reads SetInput() values for
    // these components instead of calling Eval().
    virtual bool IsInputSource() const { return false; }

    // ---- appearance (pure data, local coordinates) ----
    virtual DrawList BaseSymbol(const AttrMap&) const = 0;
    virtual bool HasLiveSymbol() const { return false; }
    virtual void BuildLiveSymbol(const EvalContext&, DrawList&) const {}
};

// A subcircuit instance never receives Eval(): the netlist builder expands it.
class SubcircuitDef final : public ComponentDef {
public:
    SubcircuitDef(CircuitId target, std::string type_name);

    CircuitId Target() const { return target_; }
    void SetPorts(std::vector<Port> ports) { ports_ = std::move(ports); }

    std::string_view Type() const override { return type_name_; }
    std::string_view Category() const override { return "Subcircuits"; }
    std::string_view DisplayName() const override { return type_name_; }
    std::vector<PinDef> Pins(const AttrMap&) const override;
    void Eval(const EvalContext&, ComponentState*, EvalResult&) const override;
    bool IsSubcircuit() const override { return true; }
    DrawList BaseSymbol(const AttrMap&) const override;

private:
    CircuitId target_;
    std::string type_name_;
    std::vector<Port> ports_;
};

}  // namespace ll
