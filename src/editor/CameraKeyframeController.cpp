
#include "CameraKeyframeController.h"
#include "../imgui/imgui.h"
#include "../imgui/imgui_benzene_widgets.h"
#include "components/keyframe_marker.h"
#include "ActionHistory.h"
#include "actions/ChangeCameraKeyframe.h"
#include "actions/DeleteCameraKeyframe.h"
#include "actions/SplitCameraKeyframeToDual.h"

CameraKeyframeController::CameraKeyframeController() : positionUniform("_cp", UniformType::Vec3), rotationUniform("_cr", UniformType::Vec2)
{
}

void CameraKeyframeController::moveCameraToKeyframe()
{
    UniformValue position = positionUniform.valueAtTime(currentTime, isEndKeyframe, 6);
    UniformValue rotation = rotationUniform.valueAtTime(currentTime, isEndKeyframe, 6);
    cameraController.position = {position.v3[0], position.v3[1], position.v3[2]};
    cameraController.xAngle = rotation.v2[0];
    cameraController.yAngle = rotation.v2[1];
    cameraController.recalculateCameraTarget();
    cameraController.markAsMoved();
}

void CameraKeyframeController::startFrame(float currentTime, bool isEnd)
{
    bool didTimeChange = this->currentTime != currentTime || isEndKeyframe != isEnd;

    cameraController.resetCameraMovementCheck();
    this->currentTime = currentTime;
    isEndKeyframe = isEnd;

    if (isLocked && didTimeChange) moveCameraToKeyframe();
}

UniformValue CameraKeyframeController::getPositionValue()
{
    UniformValue positionValue{};
    positionValue.v3[0] = cameraController.position.x;
    positionValue.v3[1] = cameraController.position.y;
    positionValue.v3[2] = cameraController.position.z;

    return positionValue;
}

UniformValue CameraKeyframeController::getRotationValue()
{
    UniformValue rotationValue{};
    rotationValue.v2[0] = cameraController.xAngle;
    rotationValue.v2[1] = cameraController.yAngle;

    return rotationValue;
}

void CameraKeyframeController::updateCamera(
    long timeDeltaMs, const KeyboardState& keyboard, const MouseState& mouse, ActionHistory& actionHistory)
{
    UniformKeyframe* positionKeyframe = positionUniform.getKeyframeAtTime(currentTime, isEndKeyframe);
    UniformKeyframe* rotationKeyframe = rotationUniform.getKeyframeAtTime(currentTime, isEndKeyframe);

    if (isLocked && positionKeyframe == nullptr)
    {
        shouldDisplayLockWarning = mouse.isDragging || keyboard.isAnyMovementKeyDown();
        return;
    }

    // didCameraMove() sometimes has forced events which we don't want to consider here
    bool didMove = cameraController.updateCamera(timeDeltaMs, keyboard, mouse);

    if (isLocked && didMove)
    {
        UniformKeyframe beforePosition = *positionKeyframe;
        std::optional<UniformKeyframe> beforeRotation = rotationKeyframe ? std::optional<UniformKeyframe>(*rotationKeyframe) : std::nullopt;

        if (rotationKeyframe == nullptr)
        {
            rotationUniform.setKeyframeAtTime(currentTime, isEndKeyframe, getRotationValue(), KeyframeInterpolation::Linear, 0.5f);
            rotationKeyframe = rotationUniform.getKeyframeAtTime(currentTime, isEndKeyframe);
        }

        positionKeyframe->value = getPositionValue();
        rotationKeyframe->value = getRotationValue();

        UniformValue afterPosition = positionKeyframe->value;
        UniformValue afterRotation = rotationKeyframe->value;

        // Lump consecutive movement frames into a single undo step by extending the most recently
        // recorded action instead of pushing a new one, as long as it still targets this same keyframe.
        auto* lastChange = dynamic_cast<ChangeCameraKeyframe*>(actionHistory.peekPast());
        if (lastChange != nullptr && lastChange->targets(currentTime, isEndKeyframe))
            lastChange->updateAfter(afterPosition, afterRotation);
        else
            actionHistory.record(std::make_unique<ChangeCameraKeyframe>(beforePosition, beforeRotation,
                UniformKeyframe{currentTime, afterPosition, beforePosition.interpolation, beforePosition.interpolationFactor},
                afterRotation, isEndKeyframe));
    }
}

void CameraKeyframeController::forceMovement()
{
    if (isLocked) moveCameraToKeyframe();
}

bool CameraKeyframeController::didCameraMove() const
{
    bool isOnKeyframe = positionUniform.hasKeyframeAtTime(currentTime);
    bool isPositionForced = isLocked && !isOnKeyframe;
    return isPositionForced || cameraController.didCameraMove();
}

void CameraKeyframeController::recalculateCameraTarget() { cameraController.recalculateCameraTarget(); }

vec3 CameraKeyframeController::getPosition() { return cameraController.position; }
vec3 CameraKeyframeController::getTarget() { return cameraController.target; }
vec3 CameraKeyframeController::getRotation() { return {cameraController.xAngle, cameraController.yAngle, 0}; }

void CameraKeyframeController::displayKeyframeMarker(ActionHistory& actionHistory)
{
    UniformKeyframe* kf = positionUniform.getKeyframeAtTime(currentTime, isEndKeyframe);

    bool isKeyframe = kf != nullptr;
    KeyframeInterpolation interpolation = isKeyframe ? kf->interpolation : KeyframeInterpolation::Linear;
    float tension = isKeyframe ? kf->interpolationFactor : 0.5f;
    bool canSplitToDual = isKeyframe && positionUniform.countKeyframesAtTime(currentTime) < 2;
    bool shouldSplitToDual = false;

    if (KeyframeMarkerWithContextMenu("Camera", &isKeyframe, &interpolation, &tension, canSplitToDual, &shouldSplitToDual))
    {
        if (!isKeyframe)
        {
            // Keyframe was deleted
            UniformKeyframe beforePosition = *kf;
            UniformKeyframe* rotationKf = rotationUniform.getKeyframeAtTime(currentTime, isEndKeyframe);
            std::optional<UniformKeyframe> beforeRotation = rotationKf ? std::optional<UniformKeyframe>(*rotationKf) : std::nullopt;

            positionUniform.removeKeyframeAtTime(currentTime, isEndKeyframe);
            rotationUniform.removeKeyframeAtTime(currentTime, isEndKeyframe);

            actionHistory.record(std::make_unique<DeleteCameraKeyframe>(currentTime, isEndKeyframe, beforePosition, beforeRotation));
        }
        else
        {
            // Keyframe was added or changed
            std::optional<UniformKeyframe> beforePosition = kf ? std::optional<UniformKeyframe>(*kf) : std::nullopt;
            UniformKeyframe* rotationKf = rotationUniform.getKeyframeAtTime(currentTime, isEndKeyframe);
            std::optional<UniformKeyframe> beforeRotation = rotationKf ? std::optional<UniformKeyframe>(*rotationKf) : std::nullopt;

            UniformValue afterPosition = getPositionValue();
            UniformValue afterRotation = getRotationValue();

            positionUniform.setKeyframeAtTime(currentTime, isEndKeyframe, afterPosition, interpolation, tension);
            rotationUniform.setKeyframeAtTime(currentTime, isEndKeyframe, afterRotation, interpolation, tension);

            actionHistory.record(std::make_unique<ChangeCameraKeyframe>(beforePosition, beforeRotation,
                UniformKeyframe{currentTime, afterPosition, interpolation, tension}, afterRotation, isEndKeyframe));
        }

        if (shouldSplitToDual && kf != nullptr)
        {
            UniformValue splitPositionValue = isKeyframe ? kf->value : getPositionValue();
            UniformKeyframe* rotationKf = rotationUniform.getKeyframeAtTime(currentTime, isEndKeyframe);
            UniformValue splitRotationValue = rotationKf != nullptr ? rotationKf->value : getRotationValue();

            positionUniform.insertKeyframeAtTime(currentTime, true, splitPositionValue, interpolation, tension);
            rotationUniform.insertKeyframeAtTime(currentTime, true, splitRotationValue, interpolation, tension);

            actionHistory.record(std::make_unique<SplitCameraKeyframeToDual>(
                currentTime, splitPositionValue, std::optional<UniformValue>(splitRotationValue), interpolation, tension));
        }
    }
}

void CameraKeyframeController::displayImGuiWindow(ActionHistory& actionHistory)
{
    ImGui::SeparatorText("Camera");
    ImGui::BeginChild("Camera");

    if (ImGui::Button(isLocked ? "Unlock" : "Lock"))
    {
        isLocked = !isLocked;
        // If the user locked the camera, reset its position
        if (isLocked) moveCameraToKeyframe();
    }

    ImGui::SameLine();
    displayKeyframeMarker(actionHistory);
    if ((cameraController.movementX != 0 || cameraController.movementY != 0 || cameraController.movementZ != 0 ||
            cameraController.movementToTarget != 0 || shouldDisplayLockWarning) &&
        isLocked)
    {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Camera is locked");
    }

    int quantization = positionUniform.quantization.value_or(6);

    ImGui::SetNextItemWidth(70);
    if (ImGui::InputInt("Quantization", &quantization, 1, 0, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        quantization = min(2, max(0, quantization));
        positionUniform.quantization = quantization;
        rotationUniform.quantization = quantization;
        if (isLocked) moveCameraToKeyframe();
    }

    ImGui::NewLine();
    ImGui::Text("WASD: Move camera");
    ImGui::Text("Q/E: Move camera up/down");
    ImGui::Text("R/F: Move camera forward/back on XZ plane");
    ImGui::Text("Left-drag: Rotate camera");

    ImGui::NewLine();
    ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", cameraController.position.x, cameraController.position.y, cameraController.position.z);
    ImGui::Text("Camera Rotation: %.2f, %.2f", cameraController.xAngle, cameraController.yAngle);
    ImGui::Text("Camera Direction: %.2f, %.2f, %.2f", cameraController.target.x - cameraController.position.x,
        cameraController.target.y - cameraController.position.y, cameraController.target.z - cameraController.position.z);

    if (ImGui::Button("Reset Camera"))
    {
        cameraController.resetCamera();
    }

    ImGui::DragFloat("Movement Speed", &cameraController.movementScale, .1f, 1.0f, 400.f);
    ImGui::EndChild();
}
