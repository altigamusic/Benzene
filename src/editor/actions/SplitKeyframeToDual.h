#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <string>

class SplitKeyframeToDual : public Action
{
    std::string uniformName;
    float time;
    UniformValue value;
    KeyframeInterpolation interpolation;
    float interpolationFactor;

  public:
    SplitKeyframeToDual(
        std::string uniformName, float time, UniformValue value, KeyframeInterpolation interpolation, float interpolationFactor);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
