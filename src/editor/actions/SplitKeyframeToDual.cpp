#include "SplitKeyframeToDual.h"
#include "../EditorState.h"

SplitKeyframeToDual::SplitKeyframeToDual(
    std::string uniformName, float time, UniformValue value, KeyframeInterpolation interpolation, float interpolationFactor)
    : uniformName(std::move(uniformName)), time(time), value(value), interpolation(interpolation), interpolationFactor(interpolationFactor)
{
}

void SplitKeyframeToDual::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->insertKeyframeAtTime(time, true, value, interpolation, interpolationFactor);
}

void SplitKeyframeToDual::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->removeKeyframeAtTime(time, true);
}

std::string SplitKeyframeToDual::describe() const { return "Split keyframe to dual"; }
