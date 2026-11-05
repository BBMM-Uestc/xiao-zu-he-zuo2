#include "CircuitFile.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <utility>

namespace logicsim {
namespace {

using Json = nlohmann::json;

Json PointToJson(const Point& point) {
    return Json{{"x", point.x}, {"y", point.y}};
}

Point PointFromJson(const Json& json) {
    return Point{
        json.at("x").get<double>(),
        json.at("y").get<double>(),
    };
}

}  // namespace

bool CircuitFile::Save(
    const CircuitDocument& document,
    const std::filesystem::path& path,
    std::string& error
) {
    try {
        Json root{
            {"version", document.version},
            {"name", document.name},
            {"components", Json::array()},
            {"wires", Json::array()},
        };

        for (const auto& component : document.components) {
            Json jsonComponent{
                {"id", component.id},
                {"type", component.type},
                {"x", component.x},
                {"y", component.y},
                {"rotation", component.rotation},
                {"properties", component.properties},
            };
            root["components"].push_back(std::move(jsonComponent));
        }

        for (const auto& wire : document.wires) {
            Json jsonWire{
                {"id", wire.id},
                {"fromComponent", wire.fromComponent},
                {"fromPin", wire.fromPin},
                {"toComponent", wire.toComponent},
                {"toPin", wire.toPin},
                {"route", Json::array()},
            };

            for (const auto& point : wire.route) {
                jsonWire["route"].push_back(PointToJson(point));
            }
            root["wires"].push_back(std::move(jsonWire));
        }

        std::ofstream output(path);
        if (!output) {
            error = "Could not open file for writing.";
            return false;
        }

        output << root.dump(2);
        if (!output) {
            error = "Could not write the complete circuit file.";
            return false;
        }

        error.clear();
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

std::optional<CircuitDocument> CircuitFile::Load(
    const std::filesystem::path& path,
    std::string& error
) {
    try {
        std::ifstream input(path);
        if (!input) {
            error = "Could not open file for reading.";
            return std::nullopt;
        }

        Json root;
        input >> root;

        CircuitDocument document;
        document.version = root.at("version").get<int>();
        if (document.version != CircuitDocument::CurrentVersion) {
            error = "Unsupported circuit file version.";
            return std::nullopt;
        }

        document.name = root.at("name").get<std::string>();
        document.components.clear();
        document.wires.clear();

        for (const auto& jsonComponent : root.at("components")) {
            ComponentInstance component;
            component.id = jsonComponent.at("id").get<ComponentId>();
            component.type = jsonComponent.at("type").get<std::string>();
            component.x = jsonComponent.at("x").get<double>();
            component.y = jsonComponent.at("y").get<double>();
            component.rotation = jsonComponent.at("rotation").get<int>();
            component.properties =
                jsonComponent.at("properties")
                    .get<std::unordered_map<std::string, std::string>>();
            document.components.push_back(std::move(component));
        }

        for (const auto& jsonWire : root.at("wires")) {
            Wire wire;
            wire.id = jsonWire.at("id").get<WireId>();
            wire.fromComponent =
                jsonWire.at("fromComponent").get<ComponentId>();
            wire.fromPin = jsonWire.at("fromPin").get<std::string>();
            wire.toComponent = jsonWire.at("toComponent").get<ComponentId>();
            wire.toPin = jsonWire.at("toPin").get<std::string>();

            for (const auto& jsonPoint : jsonWire.at("route")) {
                wire.route.push_back(PointFromJson(jsonPoint));
            }
            document.wires.push_back(std::move(wire));
        }

        error.clear();
        return document;
    } catch (const std::exception& exception) {
        error = exception.what();
        return std::nullopt;
    }
}

}  // namespace logicsim
