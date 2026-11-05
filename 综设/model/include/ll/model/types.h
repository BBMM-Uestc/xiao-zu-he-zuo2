// owner: D  stability: frozen
// Reference contract header for the logisim-lite model layer.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ll {

using ComponentId = std::uint32_t;
using WireId      = std::uint32_t;
using CircuitId   = std::uint32_t;

inline constexpr ComponentId kInvalidComponent = 0;
inline constexpr WireId      kInvalidWire      = 0;
inline constexpr CircuitId   kInvalidCircuit   = 0;

// Coordinates are half-grid units: one visible grid cell is 2 units.
// The model deliberately uses integers only.
struct GridPoint {
    int x = 0;
    int y = 0;
};

struct GridRect {
    GridPoint tl, br;

    constexpr int Width() const { return br.x - tl.x; }
    constexpr int Height() const { return br.y - tl.y; }
};

constexpr GridPoint operator+(GridPoint a, GridPoint b) { return {a.x + b.x, a.y + b.y}; }
constexpr GridPoint operator-(GridPoint a, GridPoint b) { return {a.x - b.x, a.y - b.y}; }
constexpr bool operator==(GridPoint a, GridPoint b) { return a.x == b.x && a.y == b.y; }
constexpr bool operator!=(GridPoint a, GridPoint b) { return !(a == b); }
constexpr bool operator==(GridRect a, GridRect b) { return a.tl == b.tl && a.br == b.br; }
constexpr bool operator!=(GridRect a, GridRect b) { return !(a == b); }

enum class PinDir : std::uint8_t { Input, Output, InOut };

enum class LogicState : std::uint8_t { Zero, One, Unknown, HighZ };

// ---------------------------------------------------------------------------
// Attribute value
// ---------------------------------------------------------------------------
using AttrValue = std::variant<std::int64_t, double, bool, std::string>;
using AttrMap   = std::map<std::string, AttrValue>;

// ---------------------------------------------------------------------------
// Status
// ---------------------------------------------------------------------------
enum class StatusCode {
    Ok,
    InvalidArgument,
    NotFound,
    DuplicateName,
    CycleDetected,
    ParseError,
    VersionTooNew,
    IoError,
    UnsupportedType
};

struct Status {
    StatusCode code = StatusCode::Ok;
    std::string message;  // user-facing (Chinese) text

    explicit operator bool() const { return code == StatusCode::Ok; }

    static Status Ok() { return {}; }
    static Status Fail(StatusCode code, std::string msg) {
        Status s;
        s.code = code;
        s.message = std::move(msg);
        return s;
    }
};

}  // namespace ll
