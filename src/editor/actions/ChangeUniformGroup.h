#pragma once
#include "../Action.h"
#include <string>

class ChangeUniformGroup : public Action
{
    std::string uniformName;
    std::string beforeGroup;
    std::string afterGroup;

  public:
    ChangeUniformGroup(std::string uniformName, std::string beforeGroup, std::string afterGroup);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
