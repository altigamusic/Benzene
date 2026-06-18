#include "ChangeUniformQuantization.h"
#include "../EditorState.h"

ChangeUniformQuantization::ChangeUniformQuantization(
    std::string uniformName, std::optional<int> beforeValue, std::optional<int> afterValue)
    : uniformName(std::move(uniformName)), beforeValue(beforeValue), afterValue(afterValue)
{
}

void ChangeUniformQuantization::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->quantization = afterValue;
}

void ChangeUniformQuantization::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->quantization = beforeValue;
}

std::string ChangeUniformQuantization::describe() const { return "Change quantization of " + uniformName; }
