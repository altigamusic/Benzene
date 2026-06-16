#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <string>

class MoveKeyframe : public Action
{
    std::string uniformName;
    UniformKeyframe keyframe;
    float newTime;
    bool isBeforeEnd;
    bool isAfterEnd;

  public:
    MoveKeyframe(std::string uniformName, UniformKeyframe keyframe, float newTime, bool isBeforeEnd, bool isAfterEnd);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
