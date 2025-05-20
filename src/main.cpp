#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "ext.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"
#include "intro.h"
#include <GL/gl.h>
#include <fstream>
#include <math.h>
#include <mmsystem.h>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <windows.h>

static GLuint fragmentShaderProgram = 0;
static GLuint timeLocation;

void loadFragmentShader(const char* fragmentShaderSource)
{
    int length;
    char infoLog[500];

    if (fragmentShaderProgram != 0)
    {
        glDeleteProgram(fragmentShaderProgram);
        fragmentShaderProgram = 0;
    }

    fragmentShaderProgram = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &fragmentShaderSource);

    glGetProgramInfoLog(fragmentShaderProgram, 500, &length, infoLog);

    if (length > 0) MessageBox(0, infoLog, "Result:", MB_OK | MB_ICONEXCLAMATION);

    timeLocation = glGetUniformLocation(fragmentShaderProgram, "_t");

    glUseProgram(fragmentShaderProgram);
}

void initIntro() {}

void introLoop(long timeInMs)
{
    const float ftime = 0.001f * (float)timeInMs;

    glUniform1f(timeLocation, ftime);

    glRects(-1, -1, 1, 1);
}

typedef struct
{
    //---------------
    HINSTANCE hInstance;
    HDC hDC;
    HGLRC hRC;
    HWND hWnd;
    //---------------
    int full;
    //---------------
    char wndclass[4]; // window class and title :)
                      //---------------
} WININFO;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

bool didCameraMove = false;
float xAngle = 0;
float yAngle = 0;
vec3 cameraPos = {};
vec3 cameraTarget = {};

float cameraMovementX = 0, cameraMovementY = 0, cameraMovementZ = 0, cameraMovementToTarget = 0;

float MOVEMENT_SCALE = 10;
constexpr float ANGLE_SCALE = 0.003f;
constexpr float PI = 3.141; // slightly smaller because of rounding reasons
bool isCtrlDown = false;

static const PIXELFORMATDESCRIPTOR pfd = {sizeof(PIXELFORMATDESCRIPTOR), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 8, 0, //
    0, 0, 0, 0, 0,                             // accum
    32,                                        // zbuffer
    0,                                         // stencil!
    0,                                         // aux
    PFD_MAIN_PLANE, 0, 0, 0, 0};

static WININFO wininfo = {
    0, 0, 0, 0, 0, {'i', 'q', '_', 0}
};

void recalculateAnglesFromTarget()
{
    vec3 direction = cameraTarget - cameraPos;

    xAngle = -atan2f(direction.x, direction.z);

    // Unrotate the XZ plane to get the correct YZ angle
    float s = sinf(-xAngle), c = cosf(-xAngle);
    yAngle = -atan2f(direction.y, direction.x * s + direction.z * c);
}

vec3 getCameraDirection()
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

void recalculateCameraTarget()
{
    vec3 direction = getCameraDirection();

    cameraTarget = cameraPos + direction * 10;
}

void recalculateCameraPosition()
{
    vec3 direction = getCameraDirection();

    cameraPos = cameraTarget - direction * 10;
}

static void handleMouseMovement(HWND hwndMain, UINT uMsg, WPARAM wParam, LPARAM lParam)
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

        didCameraMove = true;
        POINTS currentPoint = MAKEPOINTS(lParam);

        xDiff = (float)(currentPoint.x - start.x);
        yDiff = (float)(currentPoint.y - start.y);

        xAngle = startXAngle + xDiff * ANGLE_SCALE;
        yAngle = startYAngle + yDiff * ANGLE_SCALE;

        yAngle = min(yAngle, PI / 2);
        yAngle = max(yAngle, -PI / 2);

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

void moveForward(float amount)
{
    vec3 direction = getCameraDirection();

    // Cross up with direction to get left
    vec3 left = vec3{0, 1, 0}.cross(direction).normalize();

    // Cross left and up to get the forward
    vec3 forward = left.cross(vec3{0, 1, 0});

    cameraPos = cameraPos + (forward * amount);
    cameraTarget = cameraPos + direction;
}

void moveToTarget(float amount)
{
    vec3 direction = getCameraDirection();

    cameraPos = cameraPos + (direction * amount);
    cameraTarget = cameraPos + direction;
}

void moveLeft(float amount)
{
    vec3 direction = getCameraDirection();

    // Cross with up to get left
    vec3 left = vec3{0, 1, 0}.cross(direction).normalize();

    cameraPos = cameraPos + (left * amount);
    cameraTarget = cameraPos + direction;
}

void moveUp(float amount)
{
    cameraPos.y += amount;
    recalculateCameraTarget();
}

void updateCamera(long timeDeltaMs)
{
    float timeDelta = ((float)timeDeltaMs) / 1000.0f;
    moveToTarget(cameraMovementToTarget * timeDelta);
    moveLeft(cameraMovementX * timeDelta);
    moveUp(cameraMovementY * timeDelta);
    moveForward(cameraMovementZ * timeDelta);
}

std::string currentShader;

bool reloadFragmentShaderFromFile()
{
    const char* fragmentShaderPath = "shaders/FragmentShader.glsl";

    std::ifstream fragmentShaderFile(fragmentShaderPath);
    std::stringstream stringStream;
    stringStream << "#version 330\n"
                 << "const vec2 _res = vec2(" << RES_X << ", " << RES_Y << ");\n";

    stringStream << fragmentShaderFile.rdbuf();

    std::string s = stringStream.str();

    bool didChange = s != currentShader;

    if (didChange)
    {
        loadFragmentShader(s.c_str());
    }

    currentShader = std::move(s);

    return didChange;
}

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) return true;

    // salvapantallas
    if (uMsg == WM_SYSCOMMAND && (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER)) return 0;

    // boton x o pulsacion de escape
    if (uMsg == WM_CLOSE || uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    bool wantCaptureMouse = false;
    bool wantCaptureKeyboard = false;

    if (ImGui::GetCurrentContext() != nullptr)
    {
        ImGuiIO& io = ImGui::GetIO();
        wantCaptureMouse = io.WantCaptureMouse;
        wantCaptureKeyboard = io.WantCaptureKeyboard;
    }

    if (!wantCaptureKeyboard)
    {
        if (uMsg == WM_KEYDOWN)
        {
            if (wParam == VK_ESCAPE)
            {
                PostQuitMessage(0);
                return 0;
            }

            if (wParam == 'W')
            {
                cameraMovementToTarget = MOVEMENT_SCALE;
            }
            if (wParam == 'S')
            {
                cameraMovementToTarget = -MOVEMENT_SCALE;
            }
            if (wParam == 'A')
            {
                cameraMovementX = MOVEMENT_SCALE;
            }
            if (wParam == 'D')
            {
                cameraMovementX = -MOVEMENT_SCALE;
            }
            if (wParam == 'Q')
            {
                cameraMovementY = MOVEMENT_SCALE;
            }
            if (wParam == 'E')
            {
                cameraMovementY = -MOVEMENT_SCALE;
            }
            if (wParam == 'R')
            {
                cameraMovementZ = MOVEMENT_SCALE;
            }
            if (wParam == 'F')
            {
                cameraMovementZ = -MOVEMENT_SCALE;
            }
            if (wParam == VK_CONTROL)
            {
                isCtrlDown = true;
            }
        }

        if (uMsg == WM_KEYUP)
        {
            if (wParam == 'W' || wParam == 'S')
            {
                cameraMovementToTarget = 0;
            }
            if (wParam == 'A' || wParam == 'D')
            {
                cameraMovementX = 0;
            }
            if (wParam == 'Q' || wParam == 'E')
            {
                cameraMovementY = 0;
            }
            if (wParam == 'R' || wParam == 'F')
            {
                cameraMovementZ = 0;
            }
            if (wParam == VK_CONTROL)
            {
                isCtrlDown = false;
            }
        }
    }

    if (!wantCaptureMouse) handleMouseMovement(hWnd, uMsg, wParam, lParam);

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void window_end(WININFO* info)
{
    if (info->hRC)
    {
        wglMakeCurrent(0, 0);
        wglDeleteContext(info->hRC);
    }

    if (info->hDC) ReleaseDC(info->hWnd, info->hDC);
    if (info->hWnd) DestroyWindow(info->hWnd);

    UnregisterClass(info->wndclass, info->hInstance);

    if (info->full)
    {
        ChangeDisplaySettings(0, 0);
        ShowCursor(1);
    }
}

static int window_init(WININFO* info)
{
    unsigned int PixelFormat;
    DWORD dwExStyle, dwStyle;
    DEVMODE dmScreenSettings;
    RECT rec;

    WNDCLASS wc;

    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = info->hInstance;
    wc.lpszClassName = info->wndclass;

    if (!RegisterClass(&wc)) return 0;

    if (info->full)
    {
        dmScreenSettings.dmSize = sizeof(DEVMODE);
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmPelsWidth = RES_X;
        dmScreenSettings.dmPelsHeight = RES_Y;
        if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) return 0;
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_VISIBLE | WS_POPUP; // | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        ShowCursor(0);
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_VISIBLE | WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU;
    }

    rec.left = 0;
    rec.top = 0;
    rec.right = RES_X;
    rec.bottom = RES_Y;
    AdjustWindowRect(&rec, dwStyle, 0);

    info->hWnd = CreateWindowEx(dwExStyle, wc.lpszClassName, "Benzene", dwStyle,
        (GetSystemMetrics(SM_CXSCREEN) - rec.right + rec.left) >> 1, (GetSystemMetrics(SM_CYSCREEN) - rec.bottom + rec.top) >> 1,
        rec.right - rec.left, rec.bottom - rec.top, 0, 0, info->hInstance, 0);
    if (!info->hWnd) return 0;

    if (!(info->hDC = GetDC(info->hWnd))) return 0;

    if (!(PixelFormat = ChoosePixelFormat(info->hDC, &pfd))) return 0;

    if (!SetPixelFormat(info->hDC, PixelFormat, &pfd)) return 0;

    if (!(info->hRC = wglCreateContext(info->hDC))) return 0;

    if (!wglMakeCurrent(info->hDC, info->hRC)) return 0;

    return 1;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    int done = 0;
    WININFO* info = &wininfo;

    info->hInstance = GetModuleHandle(0);

    // if( MessageBox( 0, "fullscreen?", info->wndclass, MB_YESNO|MB_ICONQUESTION)==IDYES ) info->full++;

    if (!window_init(info))
    {
        window_end(info);
        MessageBox(0, "window_init()!", "error", MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

    ImGui::CreateContext();
    ImGui_ImplWin32_InitForOpenGL(info->hWnd);
    ImGui_ImplOpenGL3_Init();

    EXT_Init();

    initIntro();
    recalculateCameraTarget();

    float s0 = 1, s1 = 1, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
    int scene = 0;
    bool overrideControl = false;

    long prevTime = timeGetTime();
    int t = 0;
    int fpsStart = prevTime;
    int lastRerender = -1;
    int frames = 0;

    bool tick = true;
    bool prevShouldRerender = false;

    while (!done)
    {
        long currentTime = timeGetTime();
        long timeDelta = currentTime - prevTime;
        if (tick) t = min(t + timeDelta, 15500);
        prevTime = currentTime;

        didCameraMove = false;

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) done = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        didCameraMove |= cameraMovementX != 0 || cameraMovementY != 0 || cameraMovementZ != 0 || cameraMovementToTarget != 0;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Move camera by keyboard input
        updateCamera(timeDelta);

        bool shouldRerender = tick || didCameraMove;

        ImGui::Begin("Intro Params");

        if (ImGui::SliderInt("Time", &t, 0, 140000))
        {
            shouldRerender = true;
            frames = -1;
        }
        shouldRerender |= ImGui::Checkbox("Play/Pause", &tick);
        shouldRerender |= ImGui::SliderFloat("s0", &s0, 0, 1);
        shouldRerender |= ImGui::SliderFloat("s1", &s1, 0, 1);
        shouldRerender |= ImGui::SliderFloat("s2", &s2, 0, 1);
        shouldRerender |= ImGui::SliderFloat("s3", &s3, 0, 1);
        shouldRerender |= ImGui::DragFloat("s4", &s4, 0.01f);
        shouldRerender |= ImGui::DragFloat("s5", &s5, 0.01f);
        ImGui::End();

        ImGui::Begin("Camera");
        ImGui::Text("Camera Pos: %.2f, %.2f, %.2f", cameraPos.x, cameraPos.y, cameraPos.z);
        ImGui::Text("Camera Target: %.2f, %.2f, %.2f", cameraTarget.x, cameraTarget.y, cameraTarget.z);
        ImGui::Text(
            "Camera Direction: %.2f, %.2f, %.2f", cameraTarget.x - cameraPos.x, cameraTarget.y - cameraPos.y, cameraTarget.z - cameraPos.z);

        int fpsT = currentTime - fpsStart;
        ImGui::Text("Frame delta: %i ms (%.1f FPS)", frames == 0 ? 0 : (fpsT / frames), fpsT == 0 ? 0 : frames * 1000.f / fpsT);

        if (fpsT > 2000)
        {
            fpsStart = currentTime;
            frames = 0;
        }

        if (currentTime - lastRerender > 2000)
        {
            // Try reloading the file
            // TODO: Maybe change the way this is done or something
            shouldRerender |= reloadFragmentShaderFromFile();
        }

        if (ImGui::Button("Reset Camera"))
        {
            shouldRerender = true;
            cameraPos = {0, 0, 10};
            cameraTarget = {0, 0, 0};
            recalculateAnglesFromTarget();
        }

        ImGui::SliderFloat("Movement Scale", &MOVEMENT_SCALE, 1, 10);
        ImGui::End();

        // If the render stopped just now, render to the back buffer
        if (prevShouldRerender && !shouldRerender)
        {
            introLoop(t);
            SwapBuffers(info->hDC);
        }

        if (shouldRerender)
        {
            introLoop(t);
            frames++;
        }

        prevShouldRerender = shouldRerender;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(info->hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    sndPlaySound(0, 0);
    window_end(info);

    return 0;
}
