#include "CameraController.h";
#include "imgui\imgui.h";

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

void CameraController::handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static POINTS start;
    static float startXAngle;
    static float startYAngle;

    float xDiff, yDiff;

    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        SetCapture(hwndMain);
        start = MAKEPOINTS(lParam);
        startXAngle = xAngle;
        startYAngle = yAngle;
        break;
    case WM_MOUSEMOVE:
        if (!(wParam & MK_LBUTTON)) break;

        _didCameraMove = true;
        POINTS currentPoint = MAKEPOINTS(lParam);

        xDiff = (float)(currentPoint.x - start.x);
        yDiff = (float)(currentPoint.y - start.y);

        xAngle = startXAngle + xDiff * ANGLE_SCALE;
        yAngle = startYAngle + yDiff * ANGLE_SCALE;

        yAngle = min(max(yAngle, -PI / 2), PI / 2);

        // Orbit if ctrl is down
        if (isCtrlDown)
            recalculateCameraPosition();
        else
            recalculateCameraTarget();
        break;
    case WM_LBUTTONUP:
        ClipCursor(NULL);
        ReleaseCapture();
        break;
    }
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

void CameraController::updateCamera(long timeDeltaMs)
{
    float timeDelta = ((float)timeDeltaMs) / 1000.0f;
    moveToTarget(movementToTarget * timeDelta);
    moveLeft(movementX * timeDelta);
    moveUp(movementY * timeDelta);
    moveForward(movementZ * timeDelta);
}

void CameraController::handleKeyDown(WPARAM wParam)
{
    switch (wParam)
    {
    case 'W':
        movementToTarget = movementScale;
        break;
    case 'S':
        movementToTarget = -movementScale;
        break;
    case 'A':
        movementX = movementScale;
        break;
    case 'D':
        movementX = -movementScale;
        break;
    case 'Q':
        movementY = movementScale;
        break;
    case 'E':
        movementY = -movementScale;
        break;
    case 'R':
        movementZ = movementScale;
        break;
    case 'F':
        movementZ = -movementScale;
        break;
    case VK_CONTROL:
        isCtrlDown = true;
        break;
    }
}

void CameraController::handleKeyUp(WPARAM wParam)
{
    switch (wParam)
    {
    case 'W':
    case 'S':
        movementToTarget = 0;
        break;
    case 'A':
    case 'D':
        movementX = 0;
        break;
    case 'Q':
    case 'E':
        movementY = 0;
        break;
    case 'R':
    case 'F':
        movementZ = 0;
        break;
    case VK_CONTROL:
        isCtrlDown = false;
        break;
    }
}

void CameraController::resetCameraMovementCheck() { _didCameraMove = false; }

bool CameraController::didCameraMove() const
{
    return _didCameraMove || movementX != 0 || movementY != 0 || movementZ != 0 || movementToTarget != 0;
}

void CameraController::displayImGuiWindow()
{
    ImGui::SeparatorText("Camera");
    ImGui::BeginChild("Camera");
    ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", position.x, position.y, position.z);
    ImGui::Text("Camera Target: %.2f, %.2f, %.2f", target.x, target.y, target.z);
    ImGui::Text(
        "Camera Direction: %.2f, %.2f, %.2f", target.x - position.x, target.y - position.y, target.z - position.z);

    if (ImGui::Button("Reset Camera"))
    {
        position = {0, 0, 10};
        target = {0, 0, 0};
        recalculateAnglesFromTarget();
        _didCameraMove = true;
    }

    ImGui::SliderFloat("Movement Scale", &movementScale, 1, 10);
    ImGui::EndChild();
}
