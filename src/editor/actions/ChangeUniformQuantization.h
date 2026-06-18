#pragma once
#include "../Action.h"
#include <optional>
#include <string>

class ChangeUniformQuantization : public Action
{
    std::string uniformName;
    std::optional<int> beforeValue;
    std::optional<int> afterValue;

  public:
    ChangeUniformQuantization(std::string uniformName, std::optional<int> beforeValue, std::optional<int> afterValue);

    void invoke(EditorState& state) override;
    void undo(EditorState& state) override;
    std::string describe() const override;
};
