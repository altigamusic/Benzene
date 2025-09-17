#include "intro.h"
#include "windows.h"

class CameraController
{
  private:
    bool _didCameraMove = false;
    float xAngle = 0;
    float yAngle = 0;

  public:
    vec3 position = {};
    vec3 target = {};

    float movementX = 0, movementY = 0, movementZ = 0, movementToTarget = 0;
    float movementScale = 10;
    bool isCtrlDown = false;

    constexpr static float ANGLE_SCALE = 0.003f;

    CameraController();

    void recalculateAnglesFromTarget();

    vec3 getCameraDirection() const;

    void recalculateCameraTarget();

    void recalculateCameraPosition();

    void handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void moveForward(float amount);

    void moveToTarget(float amount);

    void moveLeft(float amount);

    void moveUp(float amount);

    void updateCamera(long timeDeltaMs);

    void handleKeyDown(WPARAM wParam);

    void handleKeyUp(WPARAM wParam);

    void resetCameraMovementCheck();

    bool didCameraMove() const;

    void displayImGuiWindow();
};
