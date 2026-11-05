#include "ll/model/logic_value.h"

#include <algorithm>
#include <cassert>

namespace ll {
namespace {

std::uint8_t ClampWidth(std::uint8_t w) {
    if (w < 1) return 1;
    if (w > 64) return 64;
    return w;
}

void SetBit(LogicValue& v, int i, LogicState s) {
    const std::uint64_t bit = std::uint64_t{1} << i;
    switch (s) {
        case LogicState::Zero:
            break;
        case LogicState::One:
            v.value |= bit;
            break;
        case LogicState::Unknown:
            v.xmask |= bit;
            break;
        case LogicState::HighZ:
            v.zmask |= bit;
            break;
    }
}

}  // namespace

std::uint64_t LogicMask(std::uint8_t width) {
    const std::uint8_t w = ClampWidth(width);
    if (w == 64) return ~std::uint64_t{0};
    return (std::uint64_t{1} << w) - 1;
}

LogicValue LogicValue::Zero(std::uint8_t w) {
    LogicValue v;
    v.width = ClampWidth(w);
    return v;
}

LogicValue LogicValue::One(std::uint8_t w) {
    LogicValue v;
    v.width = ClampWidth(w);
    v.value = LogicMask(v.width);
    return v;
}

LogicValue LogicValue::Unknown(std::uint8_t w) {
    LogicValue v;
    v.width = ClampWidth(w);
    v.xmask = LogicMask(v.width);
    return v;
}

LogicValue LogicValue::HighZ(std::uint8_t w) {
    LogicValue v;
    v.width = ClampWidth(w);
    v.zmask = LogicMask(v.width);
    return v;
}

LogicValue LogicValue::FromUInt(std::uint64_t value, std::uint8_t w) {
    LogicValue v;
    v.width = ClampWidth(w);
    v.value = value & LogicMask(v.width);
    return v;
}

bool LogicValue::AllKnown() const { return ((xmask | zmask) & LogicMask(width)) == 0; }

bool LogicValue::HasUnknown() const { return (xmask & LogicMask(width)) != 0; }

bool LogicValue::HasHighZ() const { return (zmask & LogicMask(width)) != 0; }

bool LogicValue::IsHighZ() const {
    const auto mask = LogicMask(width);
    return (zmask & mask) == mask;
}

LogicState LogicValue::Bit(int i) const {
    if (i < 0 || i >= static_cast<int>(width)) {
        assert(false && "LogicValue::Bit index out of range");
        return LogicState::Unknown;
    }
    const std::uint64_t bit = std::uint64_t{1} << i;
    if (zmask & bit) return LogicState::HighZ;
    if (xmask & bit) return LogicState::Unknown;
    return (value & bit) ? LogicState::One : LogicState::Zero;
}

LogicValue LogicValue::Bits(int lo, int hi) const {
    if (lo < 0 || hi < lo || hi >= static_cast<int>(width)) {
        assert(false && "LogicValue::Bits range out of bounds");
        return LogicValue::Unknown();
    }
    const int out_width = hi - lo + 1;
    LogicValue out;
    out.width = static_cast<std::uint8_t>(out_width);
    const std::uint64_t mask = LogicMask(out.width);
    out.value = (value >> lo) & mask;
    out.xmask = (xmask >> lo) & mask;
    out.zmask = (zmask >> lo) & mask;
    return out;
}

LogicValue LogicValue::Zext(std::uint8_t w) const {
    const std::uint8_t nw = ClampWidth(w);
    if (nw <= width) return Trunc(nw);
    LogicValue out = *this;
    out.width = nw;
    return out;
}

LogicValue LogicValue::Trunc(std::uint8_t w) const {
    const std::uint8_t nw = ClampWidth(w);
    if (nw >= width) return *this;
    const std::uint64_t mask = LogicMask(nw);
    LogicValue out;
    out.width = nw;
    out.value = value & mask;
    out.xmask = xmask & mask;
    out.zmask = zmask & mask;
    return out;
}

LogicValue LogicBroadcast(LogicState state, std::uint8_t width) {
    LogicValue out;
    out.width = ClampWidth(width);
    const std::uint64_t mask = LogicMask(out.width);
    switch (state) {
        case LogicState::Zero:    break;
        case LogicState::One:     out.value = mask; break;
        case LogicState::Unknown: out.xmask = mask; break;
        case LogicState::HighZ:   out.zmask = mask; break;
    }
    return out;
}

LogicValue LogicAdaptWidth(const LogicValue& v, std::uint8_t width,
                           bool broadcast_single_bit) {
    const std::uint8_t nw = ClampWidth(width);
    if (v.width == nw) return v;
    if (broadcast_single_bit && v.width == 1 && nw > 1) {
        return LogicBroadcast(v.Bit(0), nw);
    }
    if (nw < v.width) return v.Trunc(nw);
    return v.Zext(nw);
}

LogicValue LogicNot(const LogicValue& a) {
    LogicValue out = a;
    out.zmask = 0;
    out.xmask = 0;
    out.value = 0;
    for (int i = 0; i < static_cast<int>(a.width); ++i) {
        switch (a.Bit(i)) {
            case LogicState::Zero: SetBit(out, i, LogicState::One); break;
            case LogicState::One:  SetBit(out, i, LogicState::Zero); break;
            default:               SetBit(out, i, LogicState::Unknown); break;
        }
    }
    return out;
}

namespace {

template <typename Op>
LogicValue Bitwise(const LogicValue& a, const LogicValue& b, Op op) {
    const std::uint8_t width = std::max(a.width, b.width);
    const LogicValue aa = LogicAdaptWidth(a, width);
    const LogicValue bb = LogicAdaptWidth(b, width);
    LogicValue out;
    out.width = width;
    for (int i = 0; i < static_cast<int>(width); ++i) {
        SetBit(out, i, op(aa.Bit(i), bb.Bit(i)));
    }
    return out;
}

LogicState AndBit(LogicState a, LogicState b) {
    if (a == LogicState::Zero || b == LogicState::Zero) return LogicState::Zero;
    if (a == LogicState::One && b == LogicState::One) return LogicState::One;
    return LogicState::Unknown;
}

LogicState OrBit(LogicState a, LogicState b) {
    if (a == LogicState::One || b == LogicState::One) return LogicState::One;
    if (a == LogicState::Zero && b == LogicState::Zero) return LogicState::Zero;
    return LogicState::Unknown;
}

LogicState XorBit(LogicState a, LogicState b) {
    if (a == LogicState::Zero || a == LogicState::One) {
        if (b == LogicState::Zero || b == LogicState::One) {
            return (a != b) ? LogicState::One : LogicState::Zero;
        }
    }
    return LogicState::Unknown;
}

}  // namespace

LogicValue LogicAnd(const LogicValue& a, const LogicValue& b) {
    return Bitwise(a, b, AndBit);
}

LogicValue LogicOr(const LogicValue& a, const LogicValue& b) {
    return Bitwise(a, b, OrBit);
}

LogicValue LogicXor(const LogicValue& a, const LogicValue& b) {
    return Bitwise(a, b, XorBit);
}

}  // namespace ll
