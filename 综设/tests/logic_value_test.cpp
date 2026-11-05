#include "test_framework.h"

#include "ll/model/logic_value.h"

using namespace ll;

LL_TEST(logic_value_factories) {
    const LogicValue z = LogicValue::Zero(8);
    CHECK_EQ(z.width, 8);
    CHECK(z.AllKnown());
    CHECK_EQ(z.value, 0u);

    const LogicValue one = LogicValue::One(8);
    CHECK_EQ(one.value, 0xFFu);
    CHECK(one.AllKnown());

    const LogicValue x = LogicValue::Unknown(4);
    CHECK_EQ(x.width, 4);
    CHECK(x.HasUnknown());
    CHECK(!x.AllKnown());
    CHECK_EQ(x.xmask, 0xFu);

    const LogicValue hz = LogicValue::HighZ(3);
    CHECK(hz.IsHighZ());
    CHECK_EQ(hz.zmask, 0x7u);
    CHECK(!hz.AllKnown());
}

LL_TEST(logic_value_bits_and_widths) {
    const LogicValue v = LogicValue::FromUInt(0b10110100u, 8);
    CHECK(v.Bit(2) == LogicState::One);
    CHECK(v.Bit(0) == LogicState::Zero);

    const LogicValue nibble = v.Bits(4, 7);
    CHECK_EQ(nibble.width, 4);
    CHECK_EQ(nibble.value, 0xBu);

    const LogicValue widened = nibble.Zext(8);
    CHECK_EQ(widened.width, 8);
    CHECK_EQ(widened.value, 0x0Bu);

    const LogicValue narrowed = v.Trunc(4);
    CHECK_EQ(narrowed.width, 4);
    CHECK_EQ(narrowed.value, 0x4u);

    const LogicValue broadcast = LogicAdaptWidth(LogicValue::One(1), 4, true);
    CHECK_EQ(broadcast.value, 0xFu);
    CHECK_EQ(broadcast.width, 4);

    const LogicValue truncated = LogicAdaptWidth(LogicValue::FromUInt(0xAu, 4), 1, true);
    CHECK_EQ(truncated.width, 1);
    CHECK_EQ(truncated.value, 0u);
}

LL_TEST(logic_value_three_state_gates) {
    const LogicValue one = LogicValue::One(1);
    const LogicValue zero = LogicValue::Zero(1);
    const LogicValue x = LogicValue::Unknown(1);

    CHECK(LogicAnd(one, x).Bit(0) == LogicState::Unknown);
    CHECK(LogicAnd(zero, x).Bit(0) == LogicState::Zero);
    CHECK(LogicOr(one, x).Bit(0) == LogicState::One);
    CHECK(LogicOr(zero, x).Bit(0) == LogicState::Unknown);
    CHECK(LogicNot(x).Bit(0) == LogicState::Unknown);
    CHECK(LogicXor(one, x).Bit(0) == LogicState::Unknown);

    CHECK(LogicNot(zero).Bit(0) == LogicState::One);
    CHECK(LogicNot(one).Bit(0) == LogicState::Zero);

    CHECK(LogicAnd(LogicValue::HighZ(1), zero).Bit(0) == LogicState::Zero);
    CHECK(LogicOr(LogicValue::HighZ(1), one).Bit(0) == LogicState::One);
    CHECK(LogicNot(LogicValue::HighZ(1)).Bit(0) == LogicState::Unknown);
}

LL_TEST(logic_value_multi_bit_gates) {
    const LogicValue a = LogicValue::FromUInt(0b1100u, 4);
    const LogicValue b = LogicValue::FromUInt(0b1010u, 4);
    CHECK_EQ(LogicAnd(a, b).value, 0b1000u);
    CHECK_EQ(LogicOr(a, b).value, 0b1110u);
    CHECK_EQ(LogicXor(a, b).value, 0b0110u);
    CHECK_EQ(LogicNot(a).value, 0b0011u);
}
