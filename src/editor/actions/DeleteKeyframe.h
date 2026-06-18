#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <string>

class DeleteKeyframe : public Action
{
    std::string uniformName;
    float time;
    bool isEnd;
    UniformKeyframe before;

  public:
    DeleteKeyframe(std::string uniformName, float time, bool isEnd, UniformKeyframe before);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
