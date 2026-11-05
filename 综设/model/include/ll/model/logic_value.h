// owner: D (structure) / B (semantics)  stability: frozen
// Reference contract header. The semantic implementation and its tests are
// owned by B; see tests/logic_value_test.cpp.
#pragma once

#include "ll/model/types.h"

#include <cassert>
#include <cstdint>

namespace ll {

// 1..64 bit, three-state value. Each bit has exactly one of:
//   value=1, xmask=1 (unknown / conflict), zmask=1 (high impedance).
struct LogicValue {
    std::uint64_t value = 0;  // bits that are logic 1
    std::uint64_t xmask = 0;  // bits that are Unknown
    std::uint64_t zmask = 0;  // bits that are HighZ
    std::uint8_t  width = 1;  // 1..64

    static LogicValue Zero(std::uint8_t w = 1);
    static LogicValue One(std::uint8_t w = 1);
    static LogicValue Unknown(std::uint8_t w = 1);
    static LogicValue HighZ(std::uint8_t w = 1);
    static LogicValue FromUInt(std::uint64_t v, std::uint8_t w);

    bool AllKnown() const;                 // no X and no Z
    bool HasUnknown() const;               // any X
    bool HasHighZ() const;                 // any Z
    bool IsHighZ() const;                  // all bits Z
    LogicState Bit(int i) const;           // 0..width-1
    LogicValue Bits(int lo, int hi) const; // inclusive [hi:lo]
    LogicValue Zext(std::uint8_t w) const;
    LogicValue Trunc(std::uint8_t w) const;

    bool operator==(const LogicValue&) const = default;
};

// Width helpers -------------------------------------------------------------
std::uint64_t LogicMask(std::uint8_t width);

// Adapt v to exactly `width` bits. When `broadcast_single_bit` is true and v
// is one bit wide, that bit is replicated (the contract's "single bit drives a
// multi-bit net" rule). Otherwise the value is zero-extended or truncated.
LogicValue LogicAdaptWidth(const LogicValue& v, std::uint8_t width,
                           bool broadcast_single_bit = false);

// Build a value where every bit is the given state.
LogicValue LogicBroadcast(LogicState state, std::uint8_t width);

// Three-valued bitwise gates. HighZ inputs are treated as Unknown at the gate
// boundary; the gate contracts never make a gate drive high impedance.
LogicValue LogicNot(const LogicValue& a);
LogicValue LogicAnd(const LogicValue& a, const LogicValue& b);
LogicValue LogicOr(const LogicValue& a, const LogicValue& b);
LogicValue LogicXor(const LogicValue& a, const LogicValue& b);

}  // namespace ll
