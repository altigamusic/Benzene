#include "CameraController.h"
#include "../imgui/imgui.h"

constexpr static float PI = 3.141f;

CameraController::CameraController()
{
    position = {0, 0, 10};
    target = {0, 0, 0};
    recalculateAnglesFromTarget();
}

void CameraController::recalculateAnglesFromTarget()
{
    vec3 direction = target - position;

    xAngle = -atan2f(direction.x, direction.z);

    // Unrotate the XZ plane to get the correct YZ angle
    float s = sinf(-xAngle), c = cosf(-xAngle);
    yAngle = -atan2f(direction.y, direction.x * s + direction.z * c);
}

vec3 CameraController::getCameraDirection() const
{
    vec3 direction = {0, 0, 1};

    float xS = sinf(xAngle), xC = cosf(xAngle), yS = sinf(yAngle), yC = cosf(yAngle);

    // Rotate YZ plane by Y angle
    // no matrix multiplication :(
    direction = {direction.x, direction.y * yC - direction.z * yS, direction.y * yS + direction.z * yC};
    // Rotate XZ plane by X angle
    direction = {direction.x * xC - direction.z * xS, direction.y, direction.x * xS + direction.z * xC};
    return direction;
}

void CameraController::recalculateCameraTarget()
{
    vec3 direction = getCameraDirection();

    target = position + direction * 10;
}

void CameraController::recalculateCameraPosition()
{
    vec3 direction = getCameraDirection();

    position = target - direction * 10;
}

void CameraController::resetCamera()
{
    position = {0, 0, 10};
    target = {0, 0, 0};
    recalculateAnglesFromTarget();
    _didCameraMove = true;
}

void CameraController::moveForward(float amount)
{
    vec3 direction = getCameraDirection();

    // Cross up with direction to get left
    vec3 left = vec3{0, 1, 0}.cross(direction).normalize();

    // Cross left and up to get the forward
    vec3 forward = left.cross(vec3{0, 1, 0});

    position = position + (forward * amount);
    target = position + direction;
}

void CameraController::moveToTarget(float amount)
{
    vec3 direction = getCameraDirection();

    position = position + (direction * amount);
    target = position + direction;
}

void CameraController::moveLeft(float amount)
{
    vec3 direction = getCameraDirection();

    // Cross with up to get left
    vec3 left = vec3{0, 1, 0}.cross(direction).normalize();

    position = position + (left * amount);
    target = position + direction;
}

void CameraController::moveUp(float amount)
{
    position.y += amount;
    recalculateCameraTarget();
}

void CameraController::updateCamera(long timeDeltaMs, const KeyboardState& keyboard, const MouseState& mouse)
{
    bool isCtrlDown = keyboard.isDown(VK_CONTROL);
    movementToTarget = keyboard.getMovementForwards() * movementScale;
    movementX = keyboard.getMovementX() * movementScale;
    movementY = keyboard.getMovementY() * movementScale;
    movementZ = keyboard.getMovementZ() * movementScale;

    if (mouse.wasPressed)
    {
        _dragStartXAngle = xAngle;
        _dragStartYAngle = yAngle;
    }

    if (mouse.isDragging)
    {
        _didCameraMove = true;
        float xDiff = (float)(mouse.pos.x - mouse.dragStart.x);
        float yDiff = (float)(mouse.pos.y - mouse.dragStart.y);

        xAngle = _dragStartXAngle + xDiff * ANGLE_SCALE;
        yAngle = _dragStartYAngle + yDiff * ANGLE_SCALE;
        yAngle = min(max(yAngle, -PI / 2), PI / 2);

        if (isCtrlDown)
            recalculateCameraPosition();
        else
            recalculateCameraTarget();
    }

    float timeDelta = ((float)timeDeltaMs) / 1000.0f;
    moveToTarget(movementToTarget * timeDelta);
    moveLeft(movementX * timeDelta);
    moveUp(movementY * timeDelta);
    moveForward(movementZ * timeDelta);
}

void CameraController::resetCameraMovementCheck() { _didCameraMove = false; }

void CameraController::markAsMoved() { _didCameraMove = true; }

bool CameraController::didCameraMove() const
{
    return _didCameraMove || movementX != 0 || movementY != 0 || movementZ != 0 || movementToTarget != 0;
}
