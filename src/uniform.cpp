#include <windows.h>
#include <gl/GL.h>
#include "uniform.h"

UniformValue getDefault(UniformType type)
{
    UniformValue value;
    switch (type)
    {
    case UniformType::Float:
        value.f = 0.0f;
        break;
    case UniformType::Vec2:
        value.v2[0] = 0.0f;
        value.v2[1] = 0.0f;
        break;
    case UniformType::Vec3:
    case UniformType::Color:
        value.v3[0] = 0.0f;
        value.v3[1] = 0.0f;
        value.v3[2] = 0.0f;
        break;
    case UniformType::Int:
        value.i = 0;
        break;
    case UniformType::Bool:
        value.b = false;
        break;
    case UniformType::Vec4:
    default:
        value.v4[0] = 0.0f;
        value.v4[1] = 0.0f;
        value.v4[2] = 0.0f;
        value.v4[3] = 0.0f;
        break;
    }
    return value;
}

float gain(float x, float factor)
{
    float a = 0.5 * pow(2.0 * ((x < 0.5) ? x : 1.0 - x), factor);
    return (x < 0.5) ? a : 1.0 - a;
}

float tonemap(float x, float factor) { return x * (factor + 1.0) / (1.0 + factor * x); }

UniformValue Uniform::valueAtTime(float time)
{
    if (keyframes.empty()) return getDefault(type);

    auto previousKeyframe = std::find_if(keyframes.rbegin(), keyframes.rend(),
                                      [time](const UniformKeyframe& kf) { return kf.time <= time; });

    if (previousKeyframe == keyframes.rend())
        return keyframes.begin()->value;
    else if (previousKeyframe == keyframes.rbegin())
        return previousKeyframe->value;

    auto nextKeyframe = previousKeyframe.base();

    if (type != UniformType::Float)
    {
        // Non-float interpolation not supported (yet)
        return getDefault(type);
    }

    // Find the interpolation ratio between 0-1, then linearly interpolate the actual keyframe values
    float ratio;
    float x = (time - previousKeyframe->time) / (nextKeyframe->time - previousKeyframe->time);

    switch (nextKeyframe->interpolation)
    {
    case KeyframeInterpolation::Linear:
        ratio = x;
        break;
    case KeyframeInterpolation::Step:
        ratio = 0.0f;
        break;
    case KeyframeInterpolation::Gain:
        ratio = gain(x, nextKeyframe->interpolationFactor);
        break;
    case KeyframeInterpolation::Tonemap:
        ratio = tonemap(x, nextKeyframe->interpolationFactor);
        break;
    }

    float result = previousKeyframe->value.f + (nextKeyframe->value.f - previousKeyframe->value.f) * ratio;

    return {result};
}

void Uniform::setKeyframeAtTime(float time, const UniformValue& value, KeyframeInterpolation interpolation, float interpolationFactor)
{
    UniformKeyframe keyframe{};
    keyframe.time = time;
    keyframe.value = value;
    keyframe.interpolation = interpolation;
    keyframe.interpolationFactor = interpolationFactor;

    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), keyframe,
                               [](const UniformKeyframe& a, const UniformKeyframe& b) { return a.time < b.time; });

    if (it != keyframes.end() && it->time == time)
    {
        // If the keyframe already exists at this time, replace it
        *it = keyframe;
        return;
    }

    keyframes.insert(it, keyframe);
}