#include "ActionHistory.h"
#include "EditorState.h"

void ActionHistory::pushToPast(std::unique_ptr<Action> action)
{
    past.push_back(std::move(action));
    if ((int)past.size() > MAX_HISTORY) past.pop_front();
}

void ActionHistory::execute(std::unique_ptr<Action> action, EditorState& state)
{
    action->invoke(state);
    future.clear();
    pushToPast(std::move(action));
}

void ActionHistory::record(std::unique_ptr<Action> action)
{
    future.clear();
    pushToPast(std::move(action));
}

void ActionHistory::undo(EditorState& state)
{
    if (past.empty()) return;
    auto action = std::move(past.back());
    past.pop_back();
    action->undo(state);
    future.push_back(std::move(action));
}

void ActionHistory::redo(EditorState& state)
{
    if (future.empty()) return;
    auto action = std::move(future.back());
    future.pop_back();
    action->invoke(state);
    pushToPast(std::move(action));
}
