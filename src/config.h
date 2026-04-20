#pragma once

#include <vector>
#include <optional>
#include <string>
#include "uniform.h"

struct UniformConfig
{
    float bpm;
    float lengthInBeats;
    int resolutionX;
    int resolutionY;
    int shaderQuantizationDigits = 6;
    std::vector<Uniform> uniformList;
    std::optional<Uniform> cameraPosition;
    std::optional<Uniform> cameraRotation;
    std::optional<std::string> musicWavFile;
};

bool saveConfig(const UniformConfig& config, const std::string& filename);
UniformConfig loadConfig(const std::string& filename);