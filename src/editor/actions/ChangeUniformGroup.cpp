#include "ChangeUniformGroup.h"
#include "../EditorState.h"

ChangeUniformGroup::ChangeUniformGroup(std::string uniformName, std::string beforeGroup, std::string afterGroup)
    : uniformName(std::move(uniformName)), beforeGroup(std::move(beforeGroup)), afterGroup(std::move(afterGroup))
{
}

void ChangeUniformGroup::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->group = afterGroup;
}

void ChangeUniformGroup::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->group = beforeGroup;
}

std::string ChangeUniformGroup::describe() const { return "Change group of " + uniformName; }
