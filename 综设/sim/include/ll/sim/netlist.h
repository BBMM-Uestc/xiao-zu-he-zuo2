// owner: B  stability: frozen
#pragma once

#include "ll/model/circuit.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ll {
class ComponentRegistry;
class Document;
}  // namespace ll

namespace ll::sim {

struct NetId {
    std::uint32_t value = 0;

    bool valid() const { return value != 0; }
    bool operator==(const NetId&) const = default;
};

struct NetIdHash {
    std::size_t operator()(NetId id) const noexcept {
        return static_cast<std::size_t>(id.value);
    }
};

class NetlistBuilder;

// Flat "pin <-> net" structure. Net ids are 1..NetCount(); id 0 is invalid.
class Netlist {
public:
    Netlist() = default;

    std::size_t NetCount() const { return nets_.size(); }
    const std::vector<PinRef>& PinsOf(NetId) const;
    NetId NetOf(PinRef) const;
    const std::vector<NetId>& Nets() const { return nets_; }
    std::uint64_t Fingerprint() const { return fingerprint_; }

    bool empty() const { return nets_.empty(); }

private:
    friend class NetlistBuilder;

    std::vector<std::vector<PinRef>> pins_by_net_;  // pins_by_net_[0] is unused
    std::vector<NetId> nets_;
    std::unordered_map<std::uint64_t, NetId> pin_to_net_;
    std::uint64_t fingerprint_ = 0;
};

// The only sanctioned connectivity entry point. The overload taking a
// registry is additive and used by the engine/tests; the one-argument form
// uses BuiltinComponents().
Netlist BuildNetlist(const Document&);
Netlist BuildNetlist(const Document&, const ComponentRegistry&);

}  // namespace ll::sim
