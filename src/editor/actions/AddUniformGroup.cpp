#include "AddUniformGroup.h"
#include "../EditorState.h"
#include <algorithm>

AddUniformGroup::AddUniformGroup(std::string groupName) : groupName(std::move(groupName)) {}

void AddUniformGroup::invoke(EditorState& state) { state.groups.push_back(groupName); }

void AddUniformGroup::undo(EditorState& state)
{
    auto it = std::find(state.groups.begin(), state.groups.end(), groupName);
    if (it != state.groups.end()) state.groups.erase(it);
}

std::string AddUniformGroup::describe() const { return "Add group " + groupName; }
