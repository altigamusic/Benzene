#pragma once
#include "Action.h"
#include <deque>
#include <memory>
#include <vector>

struct EditorState;

class ActionHistory
{
  public:
    static constexpr int MAX_HISTORY = 500;

    void execute(std::unique_ptr<Action> action, EditorState& state);
    void record(std::unique_ptr<Action> action);
    void undo(EditorState& state);
    void redo(EditorState& state);

  private:
    std::deque<std::unique_ptr<Action>> past;
    std::vector<std::unique_ptr<Action>> future;

    void pushToPast(std::unique_ptr<Action> action);
};
