#include "SplitCameraKeyframeToDual.h"
#include "../EditorState.h"

SplitCameraKeyframeToDual::SplitCameraKeyframeToDual(float time, UniformValue positionValue, std::optional<UniformValue> rotationValue,
    KeyframeInterpolation interpolation, float interpolationFactor)
    : time(time), positionValue(positionValue), rotationValue(rotationValue), interpolation(interpolation),
      interpolationFactor(interpolationFactor)
{
}

void SplitCameraKeyframeToDual::invoke(EditorState& state)
{
    state.cameraController.positionUniform.insertKeyframeAtTime(time, true, positionValue, interpolation, interpolationFactor);
    if (rotationValue.has_value())
        state.cameraController.rotationUniform.insertKeyframeAtTime(time, true, *rotationValue, interpolation, interpolationFactor);
}

void SplitCameraKeyframeToDual::undo(EditorState& state)
{
    state.cameraController.positionUniform.removeKeyframeAtTime(time, true);
    if (rotationValue.has_value()) state.cameraController.rotationUniform.removeKeyframeAtTime(time, true);
}

std::string SplitCameraKeyframeToDual::describe() const { return "Split keyframe to dual"; }
