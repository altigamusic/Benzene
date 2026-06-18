#pragma once

#include "CameraController.h"
#include "uniform.h"

class CameraKeyframeController
{
  private:
    CameraController cameraController;
    float currentTime = 0;
    bool isLocked = true;
    bool shouldDisplayLockWarning = false;
    bool isEndKeyframe = false;

    void displayKeyframeMarker();
    UniformValue getPositionValue();
    UniformValue getRotationValue();
    void moveCameraToKeyframe();

  public:
    CameraKeyframeController();
    void startFrame(float currentTime, bool isEnd);
    void displayImGuiWindow();
    void updateCamera(long timeDeltaMs, const KeyboardState& keyboard, const MouseState& mouse);

    void recalculateCameraTarget();
    void forceMovement();
    bool didCameraMove() const;

    vec3 getPosition();
    vec3 getTarget();
    vec3 getRotation();

    Uniform positionUniform;
    Uniform rotationUniform;
};
