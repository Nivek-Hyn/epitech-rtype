#include "brickrush/Prefabs.hpp"

#include "brickrush/Components.hpp"

#include <fstream>
#include <stdexcept>

namespace brickrush {

namespace {

using Json = nlohmann::json;

// The same engine mechanism as R-Type, with this game's own components.
void registerComponents(engine::ecs::Prefabs<Json> &prefabs)
{
    prefabs.registerComponent<Box>("Box", [](const Json &json) { return Box{json.at("width"), json.at("height")}; });
    prefabs.registerComponent<Color>("Color", [](const Json &json) {
        return Color{json.value<unsigned char>("r", 255), json.value<unsigned char>("g", 255),
                     json.value<unsigned char>("b", 255)};
    });
    prefabs.registerComponent<Paddle>("Paddle", [](const Json &json) { return Paddle{json.value("speed", 600.f)}; });
    prefabs.registerComponent<Ball>("Ball", [](const Json &json) { return Ball{true, json.value("speed", 450.f)}; });
    prefabs.registerComponent<Brick>(
        "Brick", [](const Json &json) { return Brick{json.value("hits", 1), json.value("points", 10)}; });
}

} // namespace

Json readJson(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        throw std::runtime_error("Cannot open " + path);
    return Json::parse(file);
}

void loadPrefabs(engine::ecs::Prefabs<Json> &prefabs, const std::string &path)
{
    registerComponents(prefabs);
    const Json json = readJson(path);
    for (const auto &[name, components] : json.at("prefabs").items()) {
        engine::ecs::Prefabs<Json>::Components list;
        for (const auto &[component, data] : components.items())
            list.emplace_back(component, data);
        prefabs.add(name, list);
    }
}

} // namespace brickrush
