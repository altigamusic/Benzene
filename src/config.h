#pragma once

#include <vector>
#include <optional>
#include "uniform.h"

struct UniformConfig
{
    float bpm;
    float lengthInBeats;
    int resolutionX;
    int resolutionY;
    std::vector<Uniform> uniformList;
    std::optional<Uniform> cameraPosition;
    std::optional<Uniform> cameraRotation;
};

bool saveConfig(const UniformConfig& config, const std::string& filename);
UniformConfig loadConfig(const std::string& filename);