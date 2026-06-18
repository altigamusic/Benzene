#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <optional>

class SplitCameraKeyframeToDual : public Action
{
    float time;
    UniformValue positionValue;
    std::optional<UniformValue> rotationValue;
    KeyframeInterpolation interpolation;
    float interpolationFactor;

  public:
    SplitCameraKeyframeToDual(float time, UniformValue positionValue, std::optional<UniformValue> rotationValue,
        KeyframeInterpolation interpolation, float interpolationFactor);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
