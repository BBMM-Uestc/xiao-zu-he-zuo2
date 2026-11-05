#pragma once

#include "CircuitDocument.h"

#include <filesystem>
#include <optional>
#include <string>

namespace logicsim {

class CircuitFile {
public:
    [[nodiscard]] static bool Save(
        const CircuitDocument& document,
        const std::filesystem::path& path,
        std::string& error
    );

    [[nodiscard]] static std::optional<CircuitDocument> Load(
        const std::filesystem::path& path,
        std::string& error
    );
};

}  // namespace logicsim
