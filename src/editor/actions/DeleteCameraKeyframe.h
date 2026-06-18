#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <optional>

class DeleteCameraKeyframe : public Action
{
    float time;
    bool isEnd;
    UniformKeyframe beforePosition;
    std::optional<UniformKeyframe> beforeRotation;

  public:
    DeleteCameraKeyframe(float time, bool isEnd, UniformKeyframe beforePosition,
        std::optional<UniformKeyframe> beforeRotation);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
