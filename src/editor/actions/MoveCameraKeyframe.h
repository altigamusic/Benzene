#pragma once
#include "../Action.h"
#include "../uniform.h"

class MoveCameraKeyframe : public Action
{
    UniformKeyframe positionKeyframe;
    UniformKeyframe rotationKeyframe;
    float newTime;
    bool isBeforeEnd;
    bool isAfterEnd;

  public:
    MoveCameraKeyframe(
        UniformKeyframe positionKeyframe, UniformKeyframe rotationKeyframe, float newTime, bool isBeforeEnd, bool isAfterEnd);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
