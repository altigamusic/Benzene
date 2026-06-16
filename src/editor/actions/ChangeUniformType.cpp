#include "ChangeUniformType.h"
#include "../EditorState.h"

ChangeUniformType::ChangeUniformType(
    std::string uniformName, UniformType beforeType, std::vector<UniformKeyframe> beforeKeyframes, UniformType afterType)
    : uniformName(std::move(uniformName)), beforeType(beforeType), beforeKeyframes(std::move(beforeKeyframes)), afterType(afterType)
{
}

void ChangeUniformType::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->type = afterType;
    u->keyframes.clear();
}

void ChangeUniformType::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->type = beforeType;
    u->keyframes = beforeKeyframes;
}

std::string ChangeUniformType::describe() const { return "Change type of " + uniformName; }
