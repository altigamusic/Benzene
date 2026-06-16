#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <optional>

class ChangeCameraKeyframe : public Action
{
    bool isEnd;
    std::optional<UniformKeyframe> beforePosition;
    std::optional<UniformKeyframe> beforeRotation;
    UniformKeyframe afterPosition;
    UniformValue afterRotationValue;

  public:
    ChangeCameraKeyframe(std::optional<UniformKeyframe> beforePosition, std::optional<UniformKeyframe> beforeRotation,
        UniformKeyframe afterPosition, UniformValue afterRotationValue, bool isEnd);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;

    // Used to merge consecutive live-camera-movement frames into a single undo step.
    bool targets(float time, bool isEnd) const;
    void updateAfter(UniformValue newAfterPosition, UniformValue newAfterRotationValue);
};
