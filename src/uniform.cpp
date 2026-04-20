#include <windows.h>
#include <gl/GL.h>
#include "uniform.h"
#include <algorithm>
#include <stdexcept>

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

float convert01ToCorrectFactor(float interpolationFactor, KeyframeInterpolation interpolation)
{
    float a = interpolationFactor;
    switch (interpolation)
    {
    case KeyframeInterpolation::Tonemap:
        // Tonemap is -inf - inf with inflection at 0
        return (a - .5f) * 20.f;
    case KeyframeInterpolation::Gain:
        // Gain is 1 - inf (also has 0-1 but it looks bad and doesn't work)
        return a * 19.f + 1.f;
    default:
        // No factor needed for linear or step
        return 0;
    }
}

float lerp(float a, float b, float x) { return a + (b - a) * x; }

float gain(float x, float factor)
{
    float a = 0.5 * pow(2.0 * ((x < 0.5) ? x : 1.0 - x), factor);
    return (x < 0.5) ? a : 1.0 - a;
}

float tonemap(float x, float factor) { return factor > 0.f ? pow(x, factor + 1.f) : 1.f - pow(1.f - x, 1.f - factor); }

static float quantizeFloat(float value, int digits, bool binary)
{
    if (digits == -1) return value; // Skip quantization on -1

    if (binary)
    {
        if (digits >= 2) return value;

        // Truncate to only the first (digits + 2) bytes
        uint32_t intValue = *reinterpret_cast<uint32_t*>(&value);
        uint32_t mask = 0xFFFFFFFF << ((2 - digits) * 8);
        intValue &= mask;

        return *reinterpret_cast<float*>(&intValue);
    }

    if (digits == 0) return roundf(value);

    float scale = powf(10.0f, static_cast<float>(digits));
    return roundf(value * scale) / scale;
}

static UniformValue quantizeValue(UniformValue value, UniformType type, int digits, bool binary)
{
    UniformValue result = value;

    switch (type)
    {
    case UniformType::Float:
        result.f = quantizeFloat(value.f, digits, binary);
        break;
    case UniformType::Vec2:
        result.v2[0] = quantizeFloat(value.v2[0], digits, binary);
        result.v2[1] = quantizeFloat(value.v2[1], digits, binary);
        break;
    case UniformType::Vec3:
    case UniformType::Color:
        result.v3[0] = quantizeFloat(value.v3[0], digits, binary);
        result.v3[1] = quantizeFloat(value.v3[1], digits, binary);
        result.v3[2] = quantizeFloat(value.v3[2], digits, binary);
        break;
    case UniformType::Vec4:
        result.v4[0] = quantizeFloat(value.v4[0], digits, binary);
        result.v4[1] = quantizeFloat(value.v4[1], digits, binary);
        result.v4[2] = quantizeFloat(value.v4[2], digits, binary);
        result.v4[3] = quantizeFloat(value.v4[3], digits, binary);
        break;
    default:
        break;
    }

    return result;
}

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
        return gain(x, convert01ToCorrectFactor(interpolationFactor, interpolation));
    case KeyframeInterpolation::Tonemap:
        return tonemap(x, convert01ToCorrectFactor(interpolationFactor, interpolation));
    default:
        return x;
    }
}

float interpolateTime(float time, float startTime, float endTime, float prevValue, float nextValue, KeyframeInterpolation interpolation,
    float interpolationFactor, int digits, bool binaryRounding)
{
    if (endTime <= startTime) return 0.0f;

    prevValue = quantizeFloat(prevValue, digits, binaryRounding);
    nextValue = quantizeFloat(nextValue, digits, binaryRounding);

    // Find the interpolation ratio between 0-1, then linearly interpolate the actual values
    float x = (time - startTime) / (endTime - startTime);
    float ratio = interpolate0to1(x, interpolation, interpolationFactor);
    return lerp(prevValue, nextValue, ratio);
}

/// <summary>
/// Get the uniform value at the given time, based on its keyframes.
/// </summary>
/// <param name="time">The time to get the value at.</param>
/// <param name="isEnd">True if the end half should be used if there are dual keyframes at this time, and false if the start one should be
/// used.</param>
/// <param name="defaultQuantizationDigits">The default number of digits to round the keyframes to. If this value is -1, no
/// rounding is performed.</param> <returns></returns>
UniformValue Uniform::valueAtTime(float time, bool isEnd, int defaultQuantizationDigits)
{
    if (keyframes.empty()) return getDefault(type);

    int digits = defaultQuantizationDigits == -1 ? -1 : this->quantization.value_or(defaultQuantizationDigits);
    // If the uniform moves, it's going to be saved in the C++ code, so rounding should be in bytes and not decimal digits
    bool isBinaryRounding = keyframes.size() > 1;

    auto previousKeyframe =
        std::find_if(keyframes.rbegin(), keyframes.rend(), [time](const UniformKeyframe& kf) { return kf.time <= time; });

    if (previousKeyframe == keyframes.rend())
    {
        return quantizeValue(keyframes.begin()->value, type, digits, isBinaryRounding);
    }
    else if (previousKeyframe->time == time && (previousKeyframe + 1) != keyframes.rend() && (previousKeyframe + 1)->time == time)
    {
        // If the keyframe before this one has the same time, it's a dual keyframe and we just return the correct one's value
        return quantizeValue(isEnd ? previousKeyframe->value : (previousKeyframe + 1)->value, type, digits, isBinaryRounding);
    }
    else if (previousKeyframe == keyframes.rbegin())
    {
        return quantizeValue(previousKeyframe->value, type, digits, isBinaryRounding);
    }

    auto nextKeyframe = previousKeyframe.base();

    UniformValue result = getDefault(type);

    if (type == UniformType::Float)
    {
        result.f = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.f, nextKeyframe->value.f,
            nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
    }
    else if (type == UniformType::Vec2)
    {
        result.v2[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v2[0],
            nextKeyframe->value.v2[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v2[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v2[1],
            nextKeyframe->value.v2[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
    }
    else if (type == UniformType::Vec3 || type == UniformType::Color)
    {
        result.v3[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[0],
            nextKeyframe->value.v3[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v3[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[1],
            nextKeyframe->value.v3[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v3[2] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v3[2],
            nextKeyframe->value.v3[2], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
    }
    else if (type == UniformType::Vec4)
    {
        result.v4[0] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[0],
            nextKeyframe->value.v4[0], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v4[1] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[1],
            nextKeyframe->value.v4[1], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v4[2] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[2],
            nextKeyframe->value.v4[2], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
        result.v4[3] = interpolateTime(time, previousKeyframe->time, nextKeyframe->time, previousKeyframe->value.v4[3],
            nextKeyframe->value.v4[3], nextKeyframe->interpolation, nextKeyframe->interpolationFactor, digits, isBinaryRounding);
    }

    return result;
}

void Uniform::setKeyframeAtTime(
    float time, bool isEnd, const UniformValue& value, KeyframeInterpolation interpolation, float interpolationFactor)
{
    UniformKeyframe keyframe{};
    keyframe.time = time;
    keyframe.value = value;
    keyframe.interpolation = interpolation;
    keyframe.interpolationFactor = interpolationFactor;

    auto it = std::lower_bound(
        keyframes.begin(), keyframes.end(), keyframe, [](const UniformKeyframe& a, const UniformKeyframe& b) { return a.time < b.time; });

    if (it != keyframes.end() && it->time == time)
    {
        // If the keyframe already exists at this time, replace it

        // Check for duals - if this is a dual keyframe and isEnd is true, replace the end one
        auto nextKeyframe = it + 1;
        if (isEnd && nextKeyframe != keyframes.end() && nextKeyframe->time == time) it = nextKeyframe;

        *it = keyframe;
        return;
    }

    keyframes.insert(it, keyframe);
}

void Uniform::insertKeyframeAtTime(
    float time, bool isEnd, const UniformValue& value, KeyframeInterpolation interpolation, float interpolationFactor)
{
    UniformKeyframe keyframe{};
    keyframe.time = time;
    keyframe.value = value;
    keyframe.interpolation = interpolation;
    keyframe.interpolationFactor = interpolationFactor;

    auto it = std::lower_bound(
        keyframes.begin(), keyframes.end(), keyframe, [](const UniformKeyframe& a, const UniformKeyframe& b) { return a.time < b.time; });

    bool isDualKeyframeEnd = false;

    if (it != keyframes.end() && it->time == time)
    {
        if (it + 1 != keyframes.end() && (it + 1)->time == time)
        {
            throw std::runtime_error("Cannot insert dual keyframe when there are already 2 keyframes at this time");
        }

        // If the keyframe already exists at this time, insert the new one after it if isEnd is true, otherwise before it
        isDualKeyframeEnd = isEnd;
    }

    auto insertPos = isDualKeyframeEnd ? it + 1 : it;
    keyframes.insert(insertPos, keyframe);
}

int Uniform::countKeyframesAtTime(float time) const
{
    int count = 0;
    for (const auto& keyframe : keyframes)
    {
        if (keyframe.time == time)
            ++count;
        else if (keyframe.time > time)
            break;
    }
    return count;
}

UniformKeyframe* Uniform::getKeyframeAtTime(float time, bool isEnd)
{
    auto result = std::find_if(keyframes.begin(), keyframes.end(), [time](const UniformKeyframe& kf) { return kf.time == time; });
    if (result == keyframes.end()) return nullptr;

    // Check for duals - if this is a dual keyframe and isEnd is true, return the end one
    auto nextResult = result + 1;
    if (isEnd && nextResult != keyframes.end() && nextResult->time == time) return &(*nextResult);

    return &(*result);
}

bool Uniform::hasKeyframeAtTime(float time) const { return countKeyframesAtTime(time) > 0; }

bool Uniform::removeKeyframeAtTime(float time, bool isEnd)
{
    auto result = std::find_if(keyframes.begin(), keyframes.end(), [time](const UniformKeyframe& kf) { return kf.time == time; });
    if (result == keyframes.end()) return false;

    auto nextKeyframe = result + 1;
    if (isEnd && nextKeyframe != keyframes.end() && nextKeyframe->time == time)
    {
        keyframes.erase(nextKeyframe);
        return true;
    }

    keyframes.erase(result);
    return true;
}

void Uniform::setConstantValue(const UniformValue& value)
{
    keyframes.clear();
    setKeyframeAtTime(0.0f, false, value, KeyframeInterpolation::Step, 0.0f);
}
