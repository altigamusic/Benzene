#pragma once

#include <vector>
#include <optional>
#include <string>
#include "uniform.h"

struct BenzeneConfig
{
    float bpm = 120;
    float lengthInBeats = 100;
    int resolutionX = 800;
    int resolutionY = 600;
    int shaderQuantizationDigits = 6;
    bool saveCameraAsBytes = false;
    std::vector<Uniform> uniformList;
    std::optional<Uniform> cameraPosition;
    std::optional<Uniform> cameraRotation;
    std::optional<std::string> musicWavFile;
};

bool saveConfig(const BenzeneConfig& config, const std::string& filename);
BenzeneConfig loadConfig(const std::string& filename);