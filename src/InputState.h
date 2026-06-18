#pragma once
#include <windows.h>

struct KeyboardState
{
    constexpr static int KEY_FORWARD = 'W';
    constexpr static int KEY_BACKWARD = 'S';
    constexpr static int KEY_LEFT = 'A';
    constexpr static int KEY_RIGHT = 'D';
    constexpr static int KEY_UP = 'Q';
    constexpr static int KEY_DOWN = 'E';
    constexpr static int KEY_FORWARD_XZ = 'R';
    constexpr static int KEY_BACKWARD_XZ = 'F';

  private:
    bool _pressEvents[256] = {};
    bool _pressedKeys[256] = {};
    bool _heldKeys[256] = {};

  public:
    void onMessage(UINT uMsg, WPARAM wParam);
    void newFrame();
    bool isDown(int vk) const;
    bool wasKeyPressed(int vk) const;

    int getMovementForwards() const;
    int getMovementX() const;
    int getMovementY() const;
    int getMovementZ() const;
    bool isAnyMovementKeyDown() const;
};

class MouseState
{
  private:
    bool _didPressEventOccur = false;

  public:
    bool isDragging = false;
    /// <summary>
    /// True if the mouse was pressed this frame.
    /// </summary>
    bool wasPressed = false;

    POINTS pos = {};
    POINTS dragStart = {};

    void onMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    void newFrame();
};
