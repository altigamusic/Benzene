#pragma once

#include "CameraController.h"
#include "uniform.h"

class CameraKeyframeController
{
  private:
    CameraController cameraController;
    long currentTime = 0;
    bool isLocked = true;

    void displayKeyframeMarker();
    UniformValue getPositionValue();
    UniformValue getTargetValue();

  public:
    CameraKeyframeController();
    void startFrame(long currentTime);
    void displayImGuiWindow();
    void updateCamera(long timeDelta);

    bool didCameraMove() const;
    void recalculateCameraTarget();
    void handleKeyDown(WPARAM wParam);
    void handleKeyUp(WPARAM wParam);
    void handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam);

    vec3 getPosition();
    vec3 getTarget();

    Uniform positionUniform;
    Uniform targetUniform;
};
