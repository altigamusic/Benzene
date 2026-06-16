#include "RenameUniformGroup.h"
#include "../EditorState.h"
#include <algorithm>

RenameUniformGroup::RenameUniformGroup(std::string beforeName, std::string afterName)
    : beforeName(std::move(beforeName)), afterName(std::move(afterName))
{
}

static void renameGroup(EditorState& state, const std::string& from, const std::string& to)
{
    for (Uniform& u : state.config.uniformList)
        if (u.group == from) u.group = to;

    auto it = std::find(state.groups.begin(), state.groups.end(), from);
    if (it != state.groups.end()) *it = to;
}

void RenameUniformGroup::invoke(EditorState& state) { renameGroup(state, beforeName, afterName); }

void RenameUniformGroup::undo(EditorState& state) { renameGroup(state, afterName, beforeName); }

std::string RenameUniformGroup::describe() const { return "Rename group"; }
