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

float lerp(float a, float b, float x) { return a + (b - a) * x; }

float gain(float x, float factor)
{
    float a = 0.5 * pow(2.0 * ((x < 0.5) ? x : 1.0 - x), factor);
    return (x < 0.5) ? a : 1.0 - a;
}

float tonemap(float x, float factor) { return x * (factor + 1.0) / (1.0 + factor * x); }

Uniform::Uniform(const std::string& name, UniformType type) : name(name), type(type), location((GLuint)-1) {}

Uniform::Uniform(const std::string& name) : Uniform(name, UniformType::Untyped) {}

float interpolate0to1(float x, KeyframeInterpolation interpolation, float interpolationFactor)
{
    switch (interpolation)
    {
    case KeyframeInterpolation::Linear:
        return x;
    case KeyframeInterpolation::Step:
        return 0.0f;
    case KeyframeInterpolation::Gain:
        return gain(x, interpolationFactor);
    case KeyframeInterpolation::Tonemap:
        return tonemap(x, interpolationFactor);
    default:
        return x;
    }
}

float interpolateTime(float time, float startTime, float endTime, float prevValue, float nextValue, KeyframeInterpolation interpolation,
    float interpolationFactor)
{
    if (endTime <= startTime) return 0.0f;

    // Find the interpolation ratio between 0-1, then linearly interpolate the actual values
    float x = (time - startTime) / (endTime - startTime);
    float ratio = interpolate0to1(x, interpolation, interpolationFactor);
    return lerp(prevValue, nextValue, ratio);
}

UniformValue Uniform::valueAtTime(float time)
{
    if (keyframes.empty()) return getDefault(type);

    auto previousKeyframe =
        std::find_if(keyframes.rbegin(), keyframes.rend(), [time](const UniformKeyframe& kf) { return kf.time <= time; });

    if (previousKeyframe == keyframes.rend())
        return keyframes.begin()->value;
    else if (previousKeyframe == keyframes.rbegin())
        return previousKeyframe->value;

    auto nextKeyframe = previousKeyframe.base();

    UniformValue result = getDefault(type);

    if (type == UniformType::Float)
    {
        result.f = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.f, nextKeyframe->value.f,
            nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
    }
    else if (type == UniformType::Vec2)
    {
        result.v2[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v2[0],
            nextKeyframe->value.v2[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v2[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v2[1],
            nextKeyframe->value.v2[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
    }
    else if (type == UniformType::Vec3 || type == UniformType::Color)
    {
        result.v3[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[0],
            nextKeyframe->value.v3[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v3[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[1],
            nextKeyframe->value.v3[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v3[2] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[2],
            nextKeyframe->value.v3[2], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
    }
    else if (type == UniformType::Vec4)
    {
        result.v4[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[0],
            nextKeyframe->value.v4[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v4[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[1],
            nextKeyframe->value.v4[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v4[2] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[2],
            nextKeyframe->value.v4[2], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
        result.v4[3] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[3],
            nextKeyframe->value.v4[3], nextKeyframe->interpolation, nextKeyframe->interpolationFactor);
    }

    return result;
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

    void Uniform::setConstantValue(const UniformValue& value)
    {
        keyframes.clear();
        setKeyframeAtTime(0.0f, value, KeyframeInterpolation::Step, 0.0f);
    }
