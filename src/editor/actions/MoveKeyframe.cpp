#include "MoveKeyframe.h"
#include "../EditorState.h"

MoveKeyframe::MoveKeyframe(std::string uniformName, UniformKeyframe keyframe, float newTime, bool isBeforeEnd, bool isAfterEnd)
    : uniformName(std::move(uniformName)), keyframe(keyframe), newTime(newTime), isBeforeEnd(isBeforeEnd), isAfterEnd(isAfterEnd)
{
}

void MoveKeyframe::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->removeKeyframeAtTime(keyframe.time, isBeforeEnd);
    u->setKeyframeAtTime(newTime, isAfterEnd, keyframe.value, keyframe.interpolation, keyframe.interpolationFactor);
}

void MoveKeyframe::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->removeKeyframeAtTime(newTime, isAfterEnd);
    u->setKeyframeAtTime(keyframe.time, isBeforeEnd, keyframe.value, keyframe.interpolation, keyframe.interpolationFactor);
}

std::string MoveKeyframe::describe() const { return "Move keyframe on " + uniformName; }
