#pragma once
#include "../Action.h"
#include <string>

class RenameUniformGroup : public Action
{
    std::string beforeName;
    std::string afterName;

  public:
    RenameUniformGroup(std::string beforeName, std::string afterName);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
