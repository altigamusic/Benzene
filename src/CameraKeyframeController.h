#pragma once

#include "CameraController.h"
#include "uniform.h"

class CameraKeyframeController
{
  private:
    CameraController cameraController;
    float currentTime = 0;
    bool isLocked = true;

    void displayKeyframeMarker();
    UniformValue getPositionValue();
    UniformValue getRotationValue();
    void moveCameraToKeyframe();

  public:
    CameraKeyframeController();
    void startFrame(float currentTime);
    void displayImGuiWindow();
    void updateCamera(long timeDeltaMs);

    void recalculateCameraTarget();
    void handleKeyDown(WPARAM wParam);
    void handleKeyUp(WPARAM wParam);
    void handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void forceMovement();
    bool didCameraMove() const;

    vec3 getPosition();
    vec3 getTarget();
    vec3 getRotation();

    Uniform positionUniform;
    Uniform rotationUniform;
};
