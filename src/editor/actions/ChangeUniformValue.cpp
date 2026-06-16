#include "ChangeUniformValue.h"
#include "../EditorState.h"

ChangeUniformValue::ChangeUniformValue(std::string uniformName, float targetTime, bool isEnd, std::optional<UniformKeyframe> before,
    UniformValue after, KeyframeInterpolation afterInterpolation, float afterInterpolationFactor)
    : uniformName(std::move(uniformName)), targetTime(targetTime), isEnd(isEnd), before(before), after(after),
      afterInterpolation(afterInterpolation), afterInterpolationFactor(afterInterpolationFactor)
{
}

void ChangeUniformValue::invoke(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    u->setKeyframeAtTime(targetTime, isEnd, after, afterInterpolation, afterInterpolationFactor);
}

void ChangeUniformValue::undo(EditorState& state)
{
    Uniform* u = state.findUniform(uniformName);
    if (!u) return;
    if (before.has_value())
        u->setKeyframeAtTime(targetTime, isEnd, before->value, before->interpolation, before->interpolationFactor);
    else
        u->removeKeyframeAtTime(targetTime, isEnd);
}

std::string ChangeUniformValue::describe() const { return "Change " + uniformName; }
