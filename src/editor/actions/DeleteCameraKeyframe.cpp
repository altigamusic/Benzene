#include "DeleteCameraKeyframe.h"
#include "../EditorState.h"

DeleteCameraKeyframe::DeleteCameraKeyframe(
    float time, bool isEnd, UniformKeyframe beforePosition, std::optional<UniformKeyframe> beforeRotation)
    : time(time), isEnd(isEnd), beforePosition(beforePosition), beforeRotation(beforeRotation)
{
}

void DeleteCameraKeyframe::invoke(EditorState& state)
{
    state.cameraController.positionUniform.removeKeyframeAtTime(time, isEnd);
    if (beforeRotation.has_value()) state.cameraController.rotationUniform.removeKeyframeAtTime(time, isEnd);

    state.cameraController.forceMovement();
}

void DeleteCameraKeyframe::undo(EditorState& state)
{
    if (state.cameraController.positionUniform.countKeyframesAtTime(time) == 1) // The keyframe was dual before deletion
        state.cameraController.positionUniform.insertKeyframeAtTime(
            time, isEnd, beforePosition.value, beforePosition.interpolation, beforePosition.interpolationFactor);
    else
        state.cameraController.positionUniform.setKeyframeAtTime(
            time, isEnd, beforePosition.value, beforePosition.interpolation, beforePosition.interpolationFactor);

    if (beforeRotation.has_value())
    {
        if (state.cameraController.rotationUniform.countKeyframesAtTime(time) == 1) // The keyframe was dual before deletion
            state.cameraController.rotationUniform.insertKeyframeAtTime(
                time, isEnd, beforeRotation->value, beforeRotation->interpolation, beforeRotation->interpolationFactor);
        else
            state.cameraController.rotationUniform.setKeyframeAtTime(
                time, isEnd, beforeRotation->value, beforeRotation->interpolation, beforeRotation->interpolationFactor);
    }

    state.cameraController.forceMovement();
}

std::string DeleteCameraKeyframe::describe() const { return "Delete camera keyframe"; }
