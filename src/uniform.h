#pragma once
#include <windows.h>
#include "gl/GL.h"
#include <string>
#include <vector>
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

    UniformValue valueAtTime(float time);
    void setKeyframeAtTime(float time, const UniformValue& value, KeyframeInterpolation interpolation = KeyframeInterpolation::Linear,
        float interpolationFactor = 0.0f);
    UniformKeyframe* getKeyframeAtTime(float time);
    bool hasKeyframeAtTime(float time) const;
    bool removeKeyframeAtTime(float time);
    void setConstantValue(const UniformValue& value);
};
