#pragma once
#include <windows.h>
#include "gl/GL.h"
#include <string>
#include <vector>
#include <optional>
#include "uniform_data_types.h"

struct UniformKeyframe
{
    float time;
    UniformValue value;
    KeyframeInterpolation interpolation;
    float interpolationFactor;

    int interpolationToNumber() const { return static_cast<int>(interpolation); }
};

class Uniform
{
  public:
    Uniform(const std::string&, UniformType);
    Uniform(const std::string&);
    std::string name;
    UniformType type;
    GLuint location;
    std::vector<UniformKeyframe> keyframes;
    std::string group;
    std::optional<int> quantization;

    UniformValue valueAtTime(float time, bool isEnd, int defaultQuantizationDigits = -1);
    void setKeyframeAtTime(float time, bool isEnd, const UniformValue& value,
        KeyframeInterpolation interpolation = KeyframeInterpolation::Linear, float interpolationFactor = 0.0f);
    void insertKeyframeAtTime(float time, bool isEnd, const UniformValue& value,
        KeyframeInterpolation interpolation = KeyframeInterpolation::Linear, float interpolationFactor = 0.0f);
    int countKeyframesAtTime(float time) const;
    UniformKeyframe* getKeyframeAtTime(float time, bool isEnd);
    bool hasKeyframeAtTime(float time) const;
    bool removeKeyframeAtTime(float time, bool isEnd);
    void setConstantValue(const UniformValue& value);
};
