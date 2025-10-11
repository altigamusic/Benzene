#pragma once

#include "intro.h"
#include "windows.h"
#include <math.h>

typedef struct _vec3
{
    float x;
    float y;
    float z;

    inline _vec3 operator+(_vec3 a) { return {a.x + x, a.y + y, a.z + z}; }

    inline _vec3 operator+(float s) { return {x + s, y + s, z + s}; }

    inline _vec3 operator-(_vec3 a) { return {a.x - x, a.y - y, a.z - z}; }

    inline _vec3 operator-(float s) { return {x - s, y - s, z - s}; }

    inline _vec3 operator*(_vec3 a) { return {a.x * x, a.y * y, a.z * z}; }

    inline _vec3 operator*(float s) { return {x * s, y * s, z * s}; }

    inline void operator*=(_vec3 a)
    {
        x *= a.x;
        y *= a.y;
        z *= a.z;
    }

    inline void operator*=(float s)
    {
        x *= s;
        y *= s;
        z *= s;
    }

    inline struct _vec3 cross(struct _vec3 other)
    {
        return {y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x};
    }

    inline _vec3 normalize()
    {
        // Normalize
        return *this * (1.0f / sqrt(x * x + y * y + z * z));
    }
} vec3;

typedef struct _vec4
{
    float x;
    float y;
    float z;
    float w;
} vec4;

class CameraController
{
  private:
    bool _didCameraMove = false;

  public:
    vec3 position = {};
    vec3 target = {};

    float xAngle = 0;
    float yAngle = 0;
    float movementX = 0, movementY = 0, movementZ = 0, movementToTarget = 0;
    float movementScale = 10;
    bool isCtrlDown = false;

    constexpr static float ANGLE_SCALE = 0.003f;

    CameraController();

    void recalculateAnglesFromTarget();

    vec3 getCameraDirection() const;

    void recalculateCameraTarget();

    void recalculateCameraPosition();

    void resetCamera();

    void handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void handleKeyDown(WPARAM wParam);
    void handleKeyUp(WPARAM wParam);

    void moveForward(float amount);

    void moveToTarget(float amount);

    void moveLeft(float amount);

    void moveUp(float amount);

    void updateCamera(long timeDeltaMs);

    void resetCameraMovementCheck();
    void markAsMoved();
    bool didCameraMove() const;
};
