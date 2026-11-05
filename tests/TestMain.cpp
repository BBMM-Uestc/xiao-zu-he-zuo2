#include "CircuitDocument.h"
#include "CircuitFile.h"
#include "ComponentCatalog.h"
#include "SimulationEngine.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool Check(bool condition, const char* message, int& failures) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
    return condition;
}

}  // namespace

int main() {
    using namespace logicsim;

    int failures = 0;

    CircuitDocument document;
    document.name = "Round trip test";
    document.components.push_back(
        ComponentInstance{
            .id = 1,
            .type = "input",
            .x = 40.0,
            .y = 80.0,
            .rotation = 0,
            .properties = {{"label", "A"}},
        }
    );
    document.components.push_back(
        ComponentInstance{
            .id = 2,
            .type = "led",
            .x = 240.0,
            .y = 80.0,
            .rotation = 0,
            .properties = {},
        }
    );
    document.wires.push_back(
        Wire{
            .id = 1,
            .fromComponent = 1,
            .fromPin = "out",
            .toComponent = 2,
            .toPin = "in",
            .route = {{80.0, 100.0}, {240.0, 100.0}},
        }
    );

    Check(NextComponentId(document) == 3, "next component id", failures);
    Check(NextWireId(document) == 2, "next wire id", failures);

    SimulationEngine engine;
    engine.Reset(document);
    Check(
        engine.GetSnapshot().componentValues.size() == 2,
        "simulation reset creates a value per component",
        failures
    );

    Check(
        !ComponentCatalog::BuiltIns().empty(),
        "built-in component catalog is not empty",
        failures
    );

    const auto tempPath =
        std::filesystem::temp_directory_path() / "logicsim-roundtrip.json";
    std::string error;

    Check(CircuitFile::Save(document, tempPath, error), "save circuit", failures);

    const auto loaded = CircuitFile::Load(tempPath, error);
    Check(loaded.has_value(), "load circuit", failures);
    if (loaded) {
        Check(loaded->name == document.name, "round trip name", failures);
        Check(
            loaded->components.size() == document.components.size(),
            "round trip component count",
            failures
        );
        Check(
            loaded->wires.size() == document.wires.size(),
            "round trip wire count",
            failures
        );
    }

    std::error_code removeError;
    std::filesystem::remove(tempPath, removeError);

    if (failures == 0) {
        std::cout << "All LogicSim tests passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
