#include "MoveCameraKeyframe.h"
#include "../EditorState.h"

MoveCameraKeyframe::MoveCameraKeyframe(
    UniformKeyframe positionKeyframe, UniformKeyframe rotationKeyframe, float newTime, bool isBeforeEnd, bool isAfterEnd)
    : newTime(newTime), isBeforeEnd(isBeforeEnd), isAfterEnd(isAfterEnd), positionKeyframe(positionKeyframe),
      rotationKeyframe(rotationKeyframe)
{
}

void MoveCameraKeyframe::invoke(EditorState& state)
{
    state.cameraController.positionUniform.removeKeyframeAtTime(positionKeyframe.time, isBeforeEnd);
    state.cameraController.positionUniform.setKeyframeAtTime(
        newTime, isAfterEnd, positionKeyframe.value, positionKeyframe.interpolation, positionKeyframe.interpolationFactor);

    state.cameraController.rotationUniform.removeKeyframeAtTime(positionKeyframe.time, isBeforeEnd);
    state.cameraController.rotationUniform.setKeyframeAtTime(
        newTime, isAfterEnd, rotationKeyframe.value, rotationKeyframe.interpolation, rotationKeyframe.interpolationFactor);
}

void MoveCameraKeyframe::undo(EditorState& state)
{
    state.cameraController.positionUniform.removeKeyframeAtTime(newTime, isAfterEnd);
    state.cameraController.positionUniform.setKeyframeAtTime(
        positionKeyframe.time, isBeforeEnd, positionKeyframe.value, positionKeyframe.interpolation, positionKeyframe.interpolationFactor);

    state.cameraController.rotationUniform.removeKeyframeAtTime(newTime, isAfterEnd);
    state.cameraController.rotationUniform.setKeyframeAtTime(
        positionKeyframe.time, isBeforeEnd, rotationKeyframe.value, rotationKeyframe.interpolation, rotationKeyframe.interpolationFactor);
}

std::string MoveCameraKeyframe::describe() const { return "Move camera keyframe"; }
