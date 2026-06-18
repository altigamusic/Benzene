#include "InputState.h"

void KeyboardState::onMessage(UINT uMsg, WPARAM wParam)
{
    if (wParam >= 256) return;
    if (uMsg == WM_KEYDOWN)
    {
        _pressEvents[wParam] = true;
        _heldKeys[wParam] = true;
    }
    else if (uMsg == WM_KEYUP)
    {
        _heldKeys[wParam] = false;
    }
}

void KeyboardState::newFrame()
{
    memcpy(_pressedKeys, _pressEvents, sizeof(_pressedKeys));
    memset(_pressEvents, 0, sizeof(_pressEvents));
}

bool KeyboardState::isDown(int vk) const { return vk >= 0 && vk < 256 && _heldKeys[vk]; }
bool KeyboardState::wasKeyPressed(int vk) const { return vk >= 0 && vk < 256 && _pressedKeys[vk]; }

int KeyboardState::getMovementForwards() const { return isDown(KEY_FORWARD) ? 1 : isDown(KEY_BACKWARD) ? -1 : 0; }
int KeyboardState::getMovementX() const { return isDown(KEY_LEFT) ? 1 : isDown(KEY_RIGHT) ? -1 : 0; }
int KeyboardState::getMovementY() const { return isDown(KEY_UP) ? 1 : isDown(KEY_DOWN) ? -1 : 0; }
int KeyboardState::getMovementZ() const { return isDown(KEY_FORWARD_XZ) ? 1 : isDown(KEY_BACKWARD_XZ) ? -1 : 0; }
bool KeyboardState::isAnyMovementKeyDown() const
{
    return isDown(KEY_FORWARD) || isDown(KEY_BACKWARD) || isDown(KEY_LEFT) || isDown(KEY_RIGHT) || isDown(KEY_UP) || isDown(KEY_DOWN) ||
           isDown(KEY_FORWARD_XZ) || isDown(KEY_BACKWARD_XZ);
}

void MouseState::onMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_LBUTTONDOWN:
        isDragging = true;
        _didPressEventOccur = true;
        dragStart = MAKEPOINTS(lParam);
        pos = dragStart;
        SetCapture(hwnd);
        break;
    case WM_MOUSEMOVE:
        pos = MAKEPOINTS(lParam);
        break;
    case WM_LBUTTONUP:
        isDragging = false;
        ClipCursor(NULL);
        ReleaseCapture();
        break;
    }
}

void MouseState::newFrame()
{
    // Make sure wasPressed is true for exactly one frame by smoothing out the press events
    wasPressed = _didPressEventOccur;
    _didPressEventOccur = false;
}