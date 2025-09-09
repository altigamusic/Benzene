#pragma once
#include <string>
#include <vector>

enum class UniformType
{
    Untyped,
    Float,
    Vec2,
    Vec3,
    Vec4,
    Int,
    Bool,
    Color
};

union UniformValue
{
    float f;
    float v2[2];
    float v3[3];
    float v4[4];
    int i;
    bool b;
};

enum class KeyframeInterpolation
{
    Linear,
    Step,
    Tonemap,
    Gain
};

struct UniformKeyframe
{
    float time;
    UniformValue value;
    KeyframeInterpolation interpolation;
    float interpolationFactor;
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
    bool hasKeyframeAtTime(float time);
    bool removeKeyframeAtTime(float time);
    void setConstantValue(const UniformValue& value);
};
