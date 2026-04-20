#include "config.h"
#include <sstream>
#include <fstream>
#include "nlohmann/json.hpp"

constexpr int CURRENT_VERSION = 1;

using json = nlohmann::json;

static UniformType getUniformTypeFromString(const std::string& typeString)
{
    if (typeString == "float")
        return UniformType::Float;
    else if (typeString == "int")
        return UniformType::Int;
    else if (typeString == "bool")
        return UniformType::Bool;
    else if (typeString == "vec2")
        return UniformType::Vec2;
    else if (typeString == "vec3")
        return UniformType::Vec3;
    else if (typeString == "color")
        return UniformType::Color;
    else if (typeString == "vec4")
        return UniformType::Vec4;
    else
        return UniformType::Untyped;
}

static std::string uniformTypeToString(const UniformType type)
{
    switch (type)
    {
    case UniformType::Float:
        return "float";
    case UniformType::Int:
        return "int";
    case UniformType::Bool:
        return "bool";
    case UniformType::Vec2:
        return "vec2";
    case UniformType::Vec3:
        return "vec3";
    case UniformType::Color:
        return "color";
    case UniformType::Vec4:
        return "vec4";
    }

    return "unknown";
}

static UniformValue getUniformValueFromJson(const UniformType type, const json& valueJson)
{
    UniformValue value{};
    bool success = true;

    switch (type)
    {
    case UniformType::Float:
        value.f = valueJson;
        success = valueJson.is_number();
        break;
    case UniformType::Int:
        value.i = valueJson;
        success = valueJson.is_number_integer();
        break;
    case UniformType::Bool:
        value.b = valueJson;
        success = valueJson.is_boolean();
        break;
    case UniformType::Vec2:
        success = valueJson.is_array() && valueJson.size() == 2;
        value.v2[0] = valueJson[0];
        value.v2[1] = valueJson[1];
        break;
    case UniformType::Vec3:
    case UniformType::Color:
        success = valueJson.is_array() && valueJson.size() == 3;
        value.v3[0] = valueJson[0];
        value.v3[1] = valueJson[1];
        value.v3[2] = valueJson[2];
        break;
    case UniformType::Vec4:
        success = valueJson.is_array() && valueJson.size() == 4;
        value.v4[0] = valueJson[0];
        value.v4[1] = valueJson[1];
        value.v4[2] = valueJson[2];
        value.v4[3] = valueJson[3];
        break;
    default:
        success = false;
        break;
    }

    if (!success)
    {
        throw std::runtime_error("Failed to parse uniform JSON: " + valueJson.dump());
    }

    return value;
}

static json uniformValueToJson(const UniformType type, const UniformValue value)
{
    switch (type)
    {
    case UniformType::Float:
        return value.f;
    case UniformType::Int:
        return value.i;
    case UniformType::Bool:
        return value.b;
    case UniformType::Vec2:
        return json::array({value.v2[0], value.v2[1]});
    case UniformType::Vec3:
    case UniformType::Color:
        return json::array({value.v3[0], value.v3[1], value.v3[2]});
    case UniformType::Vec4:
        return json::array({value.v4[0], value.v4[1], value.v4[2], value.v4[3]});
    }

    return nullptr;
}

UniformConfig loadConfig(const std::string& filename)
{
    UniformConfig config;

    std::ifstream configFile(filename);

    json configJson = json::parse(configFile);

    config.bpm = configJson["bpm"];

    config.lengthInBeats = configJson["lengthInBeats"];
    config.resolutionX = configJson["resolution"][0];
    config.resolutionY = configJson["resolution"][1];
    if (configJson.contains("shaderQuantizationDigits") && configJson["shaderQuantizationDigits"].is_number())
    {
        config.shaderQuantizationDigits = configJson["shaderQuantizationDigits"].get<int>();
    }

    for (auto& uniformJson : configJson["uniforms"])
    {
        Uniform newUniform(uniformJson["name"], getUniformTypeFromString(uniformJson["type"]));
        newUniform.group = uniformJson.value("group", "");
        if (uniformJson.contains("quantization") && uniformJson["quantization"].is_number())
        {
            newUniform.quantization = uniformJson["quantization"].get<int>();
        }

        for (auto& keyframeJson : uniformJson["keyframes"])
        {
            int time = keyframeJson["time"];
            UniformValue value = getUniformValueFromJson(newUniform.type, keyframeJson["value"]);

            KeyframeInterpolation interpolation = static_cast<KeyframeInterpolation>((int)keyframeJson["interpolation"]);
            float tension = keyframeJson["tension"];

            if (newUniform.countKeyframesAtTime(time) >= 2)
            {
                throw std::runtime_error("Cannot insert keyframe: already dual keyframe at time " + std::to_string(time));
            }

            newUniform.insertKeyframeAtTime(time, true, value, interpolation, tension);
        }

        if (newUniform.name == "_cp")
        {
            config.cameraPosition = newUniform;
        }
        else if (newUniform.name == "_cr")
        {
            config.cameraRotation = newUniform;
        }
        else
        {
            config.uniformList.push_back(newUniform);
        }
    }

    return config;
}

json uniformToJson(const Uniform& uniform)
{
    json keyframesJson = json::array();

    for (auto& keyframe : uniform.keyframes)
    {
        keyframesJson.push_back({
            {"time", keyframe.time},
            {"value", uniformValueToJson(uniform.type, keyframe.value)},
            {"interpolation", static_cast<int>(keyframe.interpolation)},
            {"tension", keyframe.interpolationFactor}
        });
    }

    json uniformJson = {
        {"name",      uniform.name                     },
        {"type",      uniformTypeToString(uniform.type)},
        {"keyframes", keyframesJson                    },
    };
    if (!uniform.group.empty()) uniformJson["group"] = uniform.group;
    if (uniform.quantization.has_value())
    {
        uniformJson["quantization"] = uniform.quantization.value();
    }

    return uniformJson;
}

bool saveConfig(const UniformConfig& config, const std::string& filename)
{
    std::ofstream file(filename);
    if (!file.is_open())
    {
        // throw std::runtime_error("Failed to open file for saving uniforms: " + filename);
        return false;
    }

    json uniformListJson = json::array();

    for (auto& uniform : config.uniformList)
    {
        uniformListJson.push_back(uniformToJson(uniform));
    }

    if (config.cameraPosition.has_value()) uniformListJson.push_back(uniformToJson(config.cameraPosition.value()));
    if (config.cameraRotation.has_value()) uniformListJson.push_back(uniformToJson(config.cameraRotation.value()));

    json configJson = {
        {"uniforms",                 uniformListJson                                       },
        {"bpm",                      config.bpm                                            },
        {"lengthInBeats",            config.lengthInBeats                                  },
        {"resolution",               json ::array({config.resolutionX, config.resolutionY})},
        {"shaderQuantizationDigits", config.shaderQuantizationDigits                       },
        {"version",                  CURRENT_VERSION                                       },
    };

    file << std::setw(2) << configJson;

    file.close();

    return true;
}