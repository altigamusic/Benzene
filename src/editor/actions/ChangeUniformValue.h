#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <optional>
#include <string>

class ChangeUniformValue : public Action
{
    std::string uniformName;
    float targetTime;
    bool isEnd;
    std::optional<UniformKeyframe> before; // nullopt if no keyframe existed at targetTime
    UniformValue after;
    KeyframeInterpolation afterInterpolation;
    float afterInterpolationFactor;

  public:
    ChangeUniformValue(std::string uniformName, float targetTime, bool isEnd, std::optional<UniformKeyframe> before,
        UniformValue after, KeyframeInterpolation afterInterpolation, float afterInterpolationFactor);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
