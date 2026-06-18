#pragma once
#include "../Action.h"
#include <string>

class AddUniformGroup : public Action
{
    std::string groupName;

  public:
    AddUniformGroup(std::string groupName);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
