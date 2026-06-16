#include "DeleteUniform.h"
#include "../EditorState.h"
#include <algorithm>

DeleteUniform::DeleteUniform(Uniform uniform) : uniform(std::move(uniform)) {}

void DeleteUniform::invoke(EditorState& state)
{
    auto& uniformList = state.config.uniformList;
    auto it = std::find_if(uniformList.begin(), uniformList.end(), [&](const Uniform& u) { return u.name == uniform.name; });
    if (it == uniformList.end()) return;
    uniformList.erase(it);
}

void DeleteUniform::undo(EditorState& state) { state.config.uniformList.push_back(uniform); }

std::string DeleteUniform::describe() const { return "Delete " + uniform.name; }
