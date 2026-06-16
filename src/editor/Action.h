#pragma once
#include <string>

struct EditorState;

class Action
{
  public:
    virtual void invoke(EditorState&) = 0;
    virtual void undo(EditorState&) = 0;
    virtual std::string describe() const = 0;
    virtual ~Action() = default;
};
