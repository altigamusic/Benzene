#pragma once

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

float interpolate0to1(float x, KeyframeInterpolation interpolation, float interpolationFactor);