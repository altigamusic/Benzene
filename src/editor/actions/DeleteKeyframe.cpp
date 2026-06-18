#include "DeleteKeyframe.h"
#include "../EditorState.h"

DeleteKeyframe::DeleteKeyframe(std::string uniformName, float time, bool isEnd, UniformKeyframe before)
    : uniformName(std::move(uniformName)), time(time), isEnd(isEnd), before(before)
{
}

void DeleteKeyframe::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->removeKeyframeAtTime(time, isEnd);
}

void DeleteKeyframe::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;

    if (u->countKeyframesAtTime(time) == 1) // The keyframe was dual before deletion
        u->insertKeyframeAtTime(time, isEnd, before.value, before.interpolation, before.interpolationFactor);
    else
        u->setKeyframeAtTime(time, isEnd, before.value, before.interpolation, before.interpolationFactor);
}

std::string DeleteKeyframe::describe() const { return "Delete keyframe on " + uniformName; }
