#pragma once

#include <vector>
#include <optional>
#include <string>
#include "uniform.h"

struct BenzeneConfig
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

bool saveConfig(const BenzeneConfig& config, const std::string& filename);
BenzeneConfig loadConfig(const std::string& filename);