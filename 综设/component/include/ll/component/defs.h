// owner: C  stability: frozen
#pragma once

#include "ll/model/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ll {

struct PinDef {
    std::string name;
    PinDir      dir = PinDir::Input;
    std::uint8_t width = 1;
    GridPoint   offset;  // local, unrotated coordinates
};

enum class AttrType : std::uint8_t { Int, Bool, String, Enum, Color };

struct EnumItem {
    std::string value;
    std::string label;
};

struct AttrDef {
    std::string key;
    std::string label;
    AttrType    type = AttrType::Int;
    AttrValue   default_value;
    std::int64_t min = 0;
    std::int64_t max = 0;
    std::vector<EnumItem> options;
};

enum class ColorRole : std::uint8_t {
    Ink,
    Fill,
    Background,
    Accent,
    Error,
    Pin
};

struct Shape {
    enum class Kind : std::uint8_t { Line, Rect, Polyline, Circle, Arc, Text };

    Kind kind = Kind::Line;

    std::vector<GridPoint> points;  // Line / Polyline
    GridRect rect;                  // Rect / Circle / Arc / Text anchor
    int      start_deg = 0;
    int      sweep_deg = 0;
    std::string text;
    int      font_size = 12;  // 1/10 grid unit

    ColorRole stroke = ColorRole::Ink;
    ColorRole fill = ColorRole::Fill;
    int       stroke_width = 4;  // 1/10 grid unit
    bool      filled = false;
};

struct DrawList {
    std::vector<Shape> shapes;
};

}  // namespace ll
