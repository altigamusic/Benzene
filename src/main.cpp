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
#include <vector>
#include <iomanip>
#include <windows.h>
#include "CameraController.h";

#pragma region Window and Rendering Boilerplate
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

static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam)) return true;

    if (uMsg == WM_SIZE)
    {
        windowWidth = LOWORD(lParam);
        windowHeight = HIWORD(lParam);
        resetResolution();
        return 0;
    }

    // Screensaver
    if (uMsg == WM_SYSCOMMAND && (wParam == SC_SCREENSAVE || wParam == SC_MONITORPOWER)) return 0;

    // Close on close/esc button
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

            cameraController.handleKeyDown(wParam);
        }

        if (uMsg == WM_KEYUP)
        {
            cameraController.handleKeyUp(wParam);
        }
    }

    if (!wantCaptureMouse) cameraController.handleMouseMovement(hWnd, uMsg, wParam, lParam);

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

static bool window_init(WININFO* info)
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

    if (!RegisterClass(&wc)) return false;

    if (info->full)
    {
        dmScreenSettings.dmSize = sizeof(DEVMODE);
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmPelsWidth = windowWidth;
        dmScreenSettings.dmPelsHeight = windowHeight;
        if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL) return false;
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_VISIBLE | WS_POPUP; // | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        ShowCursor(0);
    }
    else
    {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_VISIBLE | WS_CAPTION | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | WS_THICKFRAME;
    }

    rec.left = 0;
    rec.top = 0;
    rec.right = windowWidth;
    rec.bottom = windowHeight;
    AdjustWindowRect(&rec, dwStyle, 0);

    info->hWnd = CreateWindowEx(dwExStyle, wc.lpszClassName, "Benzene", dwStyle,
        (GetSystemMetrics(SM_CXSCREEN) - rec.right + rec.left) >> 1, (GetSystemMetrics(SM_CYSCREEN) - rec.bottom + rec.top) >> 1,
        rec.right - rec.left, rec.bottom - rec.top, 0, 0, info->hInstance, 0);
    if (!info->hWnd) return false;

    if (!(info->hDC = GetDC(info->hWnd))) return false;

    if (!(PixelFormat = ChoosePixelFormat(info->hDC, &pfd))) return false;

    if (!SetPixelFormat(info->hDC, PixelFormat, &pfd)) return false;

    if (!(info->hRC = wglCreateContext(info->hDC))) return false;

    if (!wglMakeCurrent(info->hDC, info->hRC)) return false;

    return true;
}
#pragma endregion

static GLuint fragmentShaderProgram = 0;
static GLuint timeLocation;
static GLuint resolutionLocation = -1;

bool showDebugWindow = false;
std::string debugError;

CameraController cameraController;
std::string currentShader;

int windowWidth = 800;
int windowHeight = 600;

bool reloadFragmentShaderFromFile()
{
    const char* fragmentShaderPath = "shaders/FragmentShader.glsl";

    std::ifstream fragmentShaderFile(fragmentShaderPath);
    std::stringstream stringStream;
    stringStream << "#version 330\n"
                 << "uniform vec2 _res;";

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

void initIntro() {}

void resetResolution() { glViewport(0, 0, windowWidth, windowHeight); }

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

    debugError = std::string(infoLog, length);

    if (length > 0)
    {
        showDebugWindow = true;
        return;
    }

    timeLocation = glGetUniformLocation(fragmentShaderProgram, "_t");
    resolutionLocation = glGetUniformLocation(fragmentShaderProgram, "_res");

    resetResolution();

    glUseProgram(fragmentShaderProgram);
}

void introLoop(long timeInMs)
{
    const float ftime = 0.001f * (float)timeInMs;

    glUniform1f(timeLocation, ftime);
    glUniform2f(resolutionLocation, windowWidth, windowHeight);

    glRects(-1, -1, 1, 1);
}

void drawFpsText(float frameDelta, float fps)
{
    std::ostringstream fpsText;
    fpsText << "Frame delta: " << frameDelta << " ms (" << std::fixed << std::setprecision(1) << fps << " FPS)";

    // Draw the text at the top-left corner
    ImGui::GetForegroundDrawList()->AddText(ImVec2(10, 10), IM_COL32(0, 0, 0, 255), fpsText.str().c_str());
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    int done = 0;
    WININFO* info = &wininfo;

    info->hInstance = GetModuleHandle(0);

    if (!window_init(info))
    {
        window_end(info);
        MessageBox(0, "window_init()!", "error", MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

    ImGui::CreateContext();
    ImGui_ImplWin32_InitForOpenGL(info->hWnd);
    ImGui_ImplOpenGL3_Init();

    if (!EXT_Init())
    {
        MessageBox(0, "An OpenGL function is missing.", "Error", MB_OK | MB_ICONEXCLAMATION);
        return 0;
    }

    initIntro();
    cameraController.recalculateCameraTarget();

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
        if (tick) t += timeDelta;
        prevTime = currentTime;

        cameraController.resetCameraMovementCheck();

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) done = 1;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Move camera by keyboard input
        cameraController.updateCamera(timeDelta);

        bool shouldRerender = tick;

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

        cameraController.displayImGuiWindow();
        shouldRerender |= cameraController.didCameraMove();

        int fpsT = currentTime - fpsStart;
        drawFpsText(frames == 0 ? 0 : (fpsT / frames), fpsT == 0 ? 0 : frames * 1000.f / fpsT);

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

        if (showDebugWindow && ImGui::Begin("Shader Debug", &showDebugWindow))
        {
            ImGui::TextUnformatted(debugError.c_str());
            ImGui::End();
        }

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