// owner: B  stability: frozen
#pragma once

#include "ll/sim/netlist.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ll::sim {

struct NetlistError {
    enum class Kind : std::uint8_t {
        MultipleDrivers,  // conflicting output pins drive one net
        FloatingInput,    // an input pin sits on a net with no driver
        Oscillation,      // a combinational loop did not converge
        WidthMismatch,    // connected pins disagree on width
        FloatingNet       // a net has no driver at all
    };

    Kind kind = Kind::FloatingNet;
    std::vector<NetId> nets;
    std::string message;  // localized, directly displayable
};

const char* ErrorKindName(NetlistError::Kind kind);

}  // namespace ll::sim
