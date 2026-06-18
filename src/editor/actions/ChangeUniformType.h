#pragma once
#include "../Action.h"
#include "../uniform.h"
#include <vector>

class ChangeUniformType : public Action
{
    std::string uniformName;
    UniformType beforeType;
    std::vector<UniformKeyframe> beforeKeyframes;
    UniformType afterType;

  public:
    ChangeUniformType(
        std::string uniformName, UniformType beforeType, std::vector<UniformKeyframe> beforeKeyframes, UniformType afterType);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
