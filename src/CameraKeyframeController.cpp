
#include "CameraKeyframeController.h"
#include "imgui/imgui.h"
#include "imgui/imgui_benzene_widgets.h"

CameraKeyframeController::CameraKeyframeController() : positionUniform("_cp", UniformType::Vec3), rotationUniform("_cr", UniformType::Vec2)
{
}

void CameraKeyframeController::moveCameraToKeyframe()
{
    UniformValue position = positionUniform.valueAtTime(currentTime);
    UniformValue rotation = rotationUniform.valueAtTime(currentTime);
    cameraController.position = {position.v3[0], position.v3[1], position.v3[2]};
    cameraController.xAngle = rotation.v2[0];
    cameraController.yAngle = rotation.v2[1];
    cameraController.recalculateCameraTarget();
    cameraController.markAsMoved();
}

void CameraKeyframeController::startFrame(long currentTime)
{
    bool didTimeChange = this->currentTime != currentTime;

    cameraController.resetCameraMovementCheck();
    this->currentTime = currentTime;

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

void CameraKeyframeController::updateCamera(long timeDeltaMs)
{
    auto positionKeyframe = positionUniform.getKeyframeAtTime(currentTime);
    auto rotationKeyframe = rotationUniform.getKeyframeAtTime(currentTime);

    if (isLocked && positionKeyframe == nullptr) return;

    cameraController.updateCamera(timeDeltaMs);

    if (isLocked && cameraController.didCameraMove())
    {
        positionKeyframe->value = getPositionValue();
        if (rotationKeyframe != nullptr) rotationKeyframe->value = getRotationValue();
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

void CameraKeyframeController::handleKeyDown(WPARAM wParam) { cameraController.handleKeyDown(wParam); }
void CameraKeyframeController::handleKeyUp(WPARAM wParam) { cameraController.handleKeyUp(wParam); }
void CameraKeyframeController::handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    bool isOnKeyframe = positionUniform.hasKeyframeAtTime(currentTime);

    if (!isLocked || isOnKeyframe)
    {
        cameraController.handleMouseMovement(hwndMain, uMsg, wParam, lParam);
    }
}

vec3 CameraKeyframeController::getPosition() { return cameraController.position; }
vec3 CameraKeyframeController::getTarget() { return cameraController.target; }
vec3 CameraKeyframeController::getRotation() { return {cameraController.xAngle, cameraController.yAngle, 0}; }

void CameraKeyframeController::displayKeyframeMarker()
{
    UniformKeyframe* kf = positionUniform.getKeyframeAtTime(currentTime);
    bool isKeyframe = kf != nullptr;
    KeyframeInterpolation interpolation = isKeyframe ? kf->interpolation : KeyframeInterpolation::Linear;
    float tension = isKeyframe ? kf->interpolationFactor : 0.5f;

    if (KeyframeMarker("Camera", &isKeyframe, &interpolation, &tension))
    {
        if (!isKeyframe)
        {
            // Keyframe was deleted
            positionUniform.removeKeyframeAtTime(currentTime);
            rotationUniform.removeKeyframeAtTime(currentTime);
        }
        else
        {
            // Keyframe was added
            positionUniform.setKeyframeAtTime(currentTime, getPositionValue(), interpolation, tension);
            rotationUniform.setKeyframeAtTime(currentTime, getRotationValue(), interpolation, tension);
        }
    }
}

void CameraKeyframeController::displayImGuiWindow()
{
    ImGui::SeparatorText("Camera");
    ImGui::BeginChild("Camera");

    if (ImGui::Selectable("Locked", &isLocked) && isLocked)
        // If the user locked the camera, reset its position
        moveCameraToKeyframe();

    displayKeyframeMarker();

    ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", cameraController.position.x, cameraController.position.y, cameraController.position.z);
    ImGui::Text("Camera Target: %.2f, %.2f, %.2f", cameraController.target.x, cameraController.target.y, cameraController.target.z);
    ImGui::Text("Camera Direction: %.2f, %.2f, %.2f", cameraController.target.x - cameraController.position.x,
        cameraController.target.y - cameraController.position.y, cameraController.target.z - cameraController.position.z);

    if (ImGui::Button("Reset Camera"))
    {
        cameraController.resetCamera();
    }

    ImGui::SliderFloat("Movement Scale", &cameraController.movementScale, 1, 10);
    ImGui::EndChild();
}
