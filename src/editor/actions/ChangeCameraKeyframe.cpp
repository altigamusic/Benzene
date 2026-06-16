#include "ChangeCameraKeyframe.h"
#include "../EditorState.h"

ChangeCameraKeyframe::ChangeCameraKeyframe(std::optional<UniformKeyframe> beforePosition, std::optional<UniformKeyframe> beforeRotation,
    UniformKeyframe afterPosition, UniformValue afterRotationValue, bool isEnd)
    : isEnd(isEnd), beforePosition(beforePosition), beforeRotation(beforeRotation), afterPosition(afterPosition),
      afterRotationValue(afterRotationValue)
{
}

void ChangeCameraKeyframe::invoke(EditorState& state)
{
    state.cameraController.positionUniform.setKeyframeAtTime(
        afterPosition.time, isEnd, afterPosition.value, afterPosition.interpolation, afterPosition.interpolationFactor);
    state.cameraController.rotationUniform.setKeyframeAtTime(
        afterPosition.time, isEnd, afterRotationValue, afterPosition.interpolation, afterPosition.interpolationFactor);
    state.cameraController.forceMovement();
}

void ChangeCameraKeyframe::undo(EditorState& state)
{
    if (beforePosition.has_value())
        state.cameraController.positionUniform.setKeyframeAtTime(
            afterPosition.time, isEnd, beforePosition->value, beforePosition->interpolation, beforePosition->interpolationFactor);
    else
        state.cameraController.positionUniform.removeKeyframeAtTime(afterPosition.time, isEnd);

    if (beforeRotation.has_value())
        state.cameraController.rotationUniform.setKeyframeAtTime(
            afterPosition.time, isEnd, beforeRotation->value, beforeRotation->interpolation, beforeRotation->interpolationFactor);
    else
        state.cameraController.rotationUniform.removeKeyframeAtTime(afterPosition.time, isEnd);

    state.cameraController.forceMovement();
}

std::string ChangeCameraKeyframe::describe() const { return "Change camera keyframe"; }

bool ChangeCameraKeyframe::targets(float queryTime, bool queryIsEnd) const
{
    return afterPosition.time == queryTime && isEnd == queryIsEnd;
}

void ChangeCameraKeyframe::updateAfter(UniformValue newAfterPosition, UniformValue newAfterRotation)
{
    afterPosition.value = newAfterPosition;
    afterRotationValue = newAfterRotation;
}
