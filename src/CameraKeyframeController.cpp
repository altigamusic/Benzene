
#include "CameraKeyframeController.h"
#include "imgui/imgui.h"
#include "imgui/imgui_benzene_widgets.h"

CameraKeyframeController::CameraKeyframeController() : positionUniform("_cp", UniformType::Vec3), targetUniform("_ct", UniformType::Vec3) {}

void CameraKeyframeController::startFrame(long currentTime)
{
    cameraController.resetCameraMovementCheck();
    this->currentTime = currentTime;

    UniformValue position = positionUniform.valueAtTime(currentTime);
    UniformValue target = targetUniform.valueAtTime(currentTime);

    bool isOnKeyframe = positionUniform.hasKeyframeAtTime(currentTime);

    if (isLocked && !isOnKeyframe)
    {
        // Apply the animation values to the camera controller
        cameraController.position = {position.v3[0], position.v3[1], position.v3[2]};
        cameraController.target = {target.v3[0], target.v3[1], target.v3[2]};
        cameraController.recalculateAnglesFromTarget();
    }
}

UniformValue CameraKeyframeController::getPositionValue()
{
    UniformValue positionValue;
    positionValue.v3[0] = cameraController.position.x;
    positionValue.v3[1] = cameraController.position.y;
    positionValue.v3[2] = cameraController.position.z;

    return positionValue;
}

UniformValue CameraKeyframeController::getTargetValue()
{
    UniformValue targetValue;
    targetValue.v3[0] = cameraController.target.x;
    targetValue.v3[1] = cameraController.target.y;
    targetValue.v3[2] = cameraController.target.z;

    return targetValue;
}

void CameraKeyframeController::updateCamera(long timeDeltaMs)
{
    auto positionKeyframe = positionUniform.getKeyframeAtTime(currentTime);
    auto targetKeyframe = targetUniform.getKeyframeAtTime(currentTime);

    if (isLocked && positionKeyframe == nullptr) return;

    cameraController.updateCamera(timeDeltaMs);

    if (isLocked)
    {
        positionKeyframe->value = getPositionValue();
        if (targetKeyframe != nullptr) targetKeyframe->value = getTargetValue();
    }
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
    cameraController.handleMouseMovement(hwndMain, uMsg, wParam, lParam);
}

vec3 CameraKeyframeController::getPosition() { return cameraController.position; }
vec3 CameraKeyframeController::getTarget() { return cameraController.target; }

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
            targetUniform.removeKeyframeAtTime(currentTime);
        }
        else
        {
            // Keyframe was added
            positionUniform.setKeyframeAtTime(currentTime, getPositionValue(), interpolation, tension);
            targetUniform.setKeyframeAtTime(currentTime, getTargetValue(), interpolation, tension);
        }
    }
}

void CameraKeyframeController::displayImGuiWindow()
{
    ImGui::SeparatorText("Camera");
    ImGui::BeginChild("Camera");

    ImGui::Selectable("Locked", &isLocked);
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
