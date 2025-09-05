#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include "ext.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_opengl3.h"
#include "imgui/imgui_impl_win32.h"
#include "intro.h"
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
#include "uniform.h";
#include <regex>
#include "imgui/imgui_benzene_widgets.h"

int sidebarWidth;
int sidebarHeight;
int windowWidth = 1200;
int windowHeight = 680;
int viewportWidth = 800;
int viewportHeight = 600;
int timelineWidth;
int timelineHeight = 200;
CameraController cameraController;
const char* uniformFileName = "uniforms.txt";

bool showDemoWindow;

void resizeWindow(int width, int height)
{
    windowWidth = width;
    windowHeight = height;
    sidebarWidth = windowWidth - viewportWidth;
    sidebarHeight = viewportHeight;
    timelineWidth = windowWidth;
    timelineHeight = windowHeight - viewportHeight;

    glViewport(sidebarWidth, timelineHeight, viewportWidth, viewportHeight);
}

bool showDebugWindow = false;
std::string debugError;

bool isPlaying = true;
bool isSpaceDown;

static GLuint fragmentShaderProgram = 0;
static GLuint timeLocation;
static GLuint resolutionLocation = -1;
std::string currentShader;
std::vector<Uniform> uniformList;

void initIntro() {}

std::vector<std::string> getUndeclaredIdentifiers(std::string debugError)
{
    std::regex undeclaredIdentifierRegex(
        "Undeclared identifier:\\s*(\\w+)\\s*$|['\"](\\w+)['\"]\\s*:\\s*undeclared identifier", std::regex::icase);

    std::vector<std::string> result;
    std::smatch match;

    std::string::const_iterator searchStart(debugError.cbegin());
    while (std::regex_search(searchStart, debugError.cend(), match, undeclaredIdentifierRegex))
    {
        if (match.size() > 1)
        {
            // Only one group will match something and the other(s) will be empty, so we just add them together
            result.push_back(match[1].str() + match[2].str());
        }
        searchStart = match.suffix().first;
    }

    return result;
}

std::string generateUniformCode()
{
    std::stringstream uniformStream;
    for (const Uniform& uniform : uniformList)
    {
        switch (uniform.type)
        {
        case UniformType::Float:
            uniformStream << "uniform float " << uniform.name << ";\n";
            break;
        case UniformType::Int:
            uniformStream << "uniform int " << uniform.name << ";\n";
            break;
        case UniformType::Bool:
            uniformStream << "uniform bool " << uniform.name << ";\n";
            break;
        case UniformType::Vec2:
            uniformStream << "uniform vec2 " << uniform.name << ";\n";
            break;
        case UniformType::Vec3:
        case UniformType::Color:
            uniformStream << "uniform vec3 " << uniform.name << ";\n";
            break;
        case UniformType::Vec4:
            uniformStream << "uniform vec4 " << uniform.name << ";\n";
            break;
        }
    }
    return uniformStream.str();
}

void saveUniformsToFile(const std::string& filename)
{
    /*std::ofstream file(filename);
    if (!file.is_open())
    {
        debugError = "Failed to open file for saving uniforms: " + filename;
        showDebugWindow = true;
        return;
    }
    for (const Uniform& uniform : uniformList)
    {
        switch (uniform.type)
        {
        case UniformType::Float:
            file << uniform.name << ";float;" << std::fixed << std::setprecision(6) << uniform.value.f << "\n";
            break;
        case UniformType::Int:
            file << uniform.name << ";int;" << uniform.value.i << "\n";
            break;
        case UniformType::Bool:
            file << uniform.name << ";bool;" << (uniform.value.b ? "true" : "false") << "\n";
            break;
        case UniformType::Vec2:
            file << uniform.name << ";vec2;" << std::fixed << std::setprecision(6) << uniform.value.v2[0] << "," << uniform.value.v2[1]
                 << "\n";
            break;
        case UniformType::Vec3:
            file << uniform.name << ";vec3;" << uniform.value.v3[0] << "," << uniform.value.v3[1] << "," << uniform.value.v3[2] << "\n";
            break;
        case UniformType::Color:
            file << uniform.name << ";color;" << uniform.value.v3[0] << "," << uniform.value.v3[1] << "," << uniform.value.v3[2] << "\n";
            break;
        case UniformType::Vec4:
            file << uniform.name << ";vec4;" << uniform.value.v4[0] << "," << uniform.value.v4[1] << "," << uniform.value.v4[2] << ","
                 << uniform.value.v4[3] << "\n";
            break;
        }
    }
    file.close();*/
}

void loadFragmentShader(std::string fragmentShaderSource, bool didTryInjecting = false)
{
    int length;
    char infoLog[500];

    if (fragmentShaderProgram != 0)
    {
        glDeleteProgram(fragmentShaderProgram);
        fragmentShaderProgram = 0;
    }

    std::string source =
        "#version 330\n"
        "uniform vec2 _res;\n"
        "uniform float _t;\n" +
        generateUniformCode() + fragmentShaderSource;

    const char* srcPtr = source.c_str();
    fragmentShaderProgram = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &srcPtr);

    glGetProgramInfoLog(fragmentShaderProgram, 500, &length, infoLog);

    debugError = std::string(infoLog, length);

    if (length > 0)
    {
        if (!didTryInjecting)
        {
            auto undeclaredIdentifiers = getUndeclaredIdentifiers(debugError);

            // Inject all undeclared identifiers as uniforms and recompile
            for (const std::string& name : undeclaredIdentifiers)
            {
                uniformList.push_back(Uniform(name, UniformType::Float));
            }

            return loadFragmentShader(fragmentShaderSource, true);
        }

        showDebugWindow = true;
        return;
    }
    else
    {
        showDebugWindow = false;
    }

    timeLocation = glGetUniformLocation(fragmentShaderProgram, "_t");
    resolutionLocation = glGetUniformLocation(fragmentShaderProgram, "_res");

    resizeWindow(windowWidth, windowHeight);

    for (Uniform& uniform : uniformList)
    {
        uniform.location = glGetUniformLocation(fragmentShaderProgram, uniform.name.c_str());
        /*if (uniform.location == -1)
        {
            debugError = "Uniform '" + uniform.name + "' not found in shader!";
            showDebugWindow = true;
        }*/
    }

    glUseProgram(fragmentShaderProgram);
}

bool reloadFragmentShaderFromFile()
{
    const char* fragmentShaderPath = "shaders/FragmentShader.glsl";

    std::ifstream fragmentShaderFile(fragmentShaderPath);
    std::stringstream stringStream;
    stringStream << fragmentShaderFile.rdbuf();

    std::string s = stringStream.str();

    bool didChange = s != currentShader;

    if (didChange && !s.empty())
    {
        loadFragmentShader(s.c_str());
    }

    currentShader = std::move(s);

    return didChange;
}

void introLoop(long timeInMs)
{
    const float ftime = 0.001f * (float)timeInMs;

    glUniform1f(timeLocation, ftime);
    glUniform2f(resolutionLocation, viewportWidth, viewportHeight);

    for (Uniform& uniform : uniformList)
    {
        auto value = uniform.valueAtTime(timeInMs);

        switch (uniform.type)
        {
        case UniformType::Float:
            glUniform1f(uniform.location, value.f);
            break;
        case UniformType::Int:
            glUniform1i(uniform.location, value.i);
            break;
        case UniformType::Bool:
            glUniform1i(uniform.location, value.b ? 1 : 0);
            break;
        case UniformType::Vec2:
            glUniform2f(uniform.location, value.v2[0], value.v2[1]);
            break;
        case UniformType::Vec3:
        case UniformType::Color:
            glUniform3f(uniform.location, value.v3[0], value.v3[1], value.v3[2]);
            break;
        case UniformType::Vec4:
            glUniform4f(uniform.location, value.v4[0], value.v4[1], value.v4[2], value.v4[3]);
            break;
        }
    }

    glRects(-1, -1, 1, 1);
}

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
        resizeWindow(LOWORD(lParam), HIWORD(lParam));
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
            else if (wParam == VK_F1)
            {
                showDemoWindow = true;
            }
            else if (wParam == VK_SPACE && !isSpaceDown)
            {
                // Prevent holding space from retriggering a bunch of times
                isSpaceDown = true;
                isPlaying = !isPlaying;
            }

            cameraController.handleKeyDown(wParam);
        }

        if (uMsg == WM_KEYUP)
        {
            if (wParam == VK_SPACE) isSpaceDown = false;
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

bool renderAndUpdateUniforms(float time)
{
    if (uniformList.empty()) return false;

    bool didAnythingChange = false;

    ImGui::SeparatorText("Uniforms");
    ImGui::BeginChild("Uniforms", ImVec2(0, sidebarHeight / 2));

    for (Uniform& uniform : uniformList)
    {
        UniformValue value = uniform.valueAtTime(time);
        bool didThisUniformChange = false;

        switch (uniform.type)
        {
        case UniformType::Float:
            didThisUniformChange = ImGui::DragFloat(uniform.name.c_str(), &value.f, 0.005f);
            break;
        case UniformType::Int:
            didThisUniformChange = ImGui::DragInt(uniform.name.c_str(), &value.i, 0.005f);
            break;
        case UniformType::Bool:
            didThisUniformChange = ImGui::Checkbox(uniform.name.c_str(), &value.b);
            break;
        case UniformType::Vec2:
            didThisUniformChange = DragVector2(uniform.name.c_str(), (ImVec2*)(&value.v2), 0.005f);
            break;
        case UniformType::Color:
            didThisUniformChange = ImGui::ColorPicker3(uniform.name.c_str(), value.v3);
            break;
        }

        // If the uniform changed, add a keyframe at the current time
        if (didThisUniformChange)
        {
            uniform.setKeyframeAtTime(time, value);
            didAnythingChange = true;
        }
    }
    ImGui::EndChild();

    return didAnythingChange;
}

void loadUniformsFromFile(const std::string& filename)
{
    uniformList.clear();

    std::ifstream file(filename);
    if (!file.is_open())
    {
        // The uniform file is optional
        return;
    }

    std::string line;

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string name, type, valueStr;
        bool loadError = false;

        if (std::getline(ss, name, ';') && std::getline(ss, type, ';') && std::getline(ss, valueStr))
        {
            Uniform uniform(name);
            UniformValue value;

            try
            {
                if (type == "float")
                {
                    uniform.type = UniformType::Float;
                    value.f = std::stof(valueStr);
                }
                else if (type == "int")
                {
                    uniform.type = UniformType::Int;
                    value.i = std::stoi(valueStr);
                }
                else if (type == "bool")
                {
                    uniform.type = UniformType::Bool;
                    value.b = (valueStr == "true");
                    loadError = valueStr != "false" && valueStr != "true";
                }
                else if (type == "vec2")
                {
                    uniform.type = UniformType::Vec2;
                    loadError = sscanf(valueStr.c_str(), "%f,%f", &value.v2[0], &value.v2[1]) != 2;
                }
                else if (type == "vec3" || type == "color")
                {
                    uniform.type = type == "vec3" ? UniformType::Vec3 : UniformType::Color;
                    loadError = sscanf(valueStr.c_str(), "%f,%f,%f", &value.v3[0], &value.v3[1], &value.v3[2]) != 3;
                }
                else if (type == "vec4")
                {
                    uniform.type = UniformType::Vec4;
                    loadError = sscanf(valueStr.c_str(), "%f,%f,%f,%f", &value.v4[0], &value.v4[1], &value.v4[2], &value.v4[3]) != 4;
                }
                else
                {
                    loadError = true;
                }
            }
            catch (const std::invalid_argument&)
            {
                loadError = true;
            }
            catch (const std::out_of_range&)
            {
                loadError = true;
            }

            if (loadError || uniform.type == UniformType::Untyped)
            {
                debugError = "Corrupt uniform file, ignoring";
                showDebugWindow = true;
                uniformList.clear();
                return;
            }

            uniform.setConstantValue(value);
            uniformList.push_back(uniform);
        }
    }
    file.close();
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    bool done = false;
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
    loadUniformsFromFile(uniformFileName);

    long prevTime = timeGetTime();
    int t = 0;
    int fpsStart = prevTime;
    int lastRerender = -1;
    int frames = 0;

    bool prevShouldRerender = false;

    while (!done)
    {
        long currentTime = timeGetTime();
        long timeDelta = currentTime - prevTime;
        if (isPlaying) t += timeDelta;
        prevTime = currentTime;

        cameraController.resetCameraMovementCheck();

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) done = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);

        // Move camera by keyboard input
        cameraController.updateCamera(timeDelta);

        bool shouldRerender = isPlaying;

        ImGui::SetNextWindowPos(ImVec2(0, viewportHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(timelineWidth, timelineHeight), ImGuiCond_Always);

        ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        if (ImGui::SliderInt("##", &t, 0, 140000))
        {
            shouldRerender = true;
            frames = -1;
        }
        ImGui::SameLine();

        if (ImGui::Button(isPlaying ? "Pause" : "Play"))
        {
            isPlaying = !isPlaying;
            shouldRerender = true;
        }
        ImGui::SetItemTooltip("Space");

        ImGui::End();

        // Sidebar
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight), ImGuiCond_Always);

        ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
        shouldRerender |= renderAndUpdateUniforms(t);

        cameraController.displayImGuiWindow();
        shouldRerender |= cameraController.didCameraMove();

        int fpsT = currentTime - fpsStart;
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() * 2);
        ImGui::Text("Frame delta: %.3f ms (%.1f FPS)\n", frames == 0 ? 0 : ((float)fpsT / frames), fpsT == 0 ? 0 : frames * 1000.f / fpsT);

        if (fpsT > 2000)
        {
            fpsStart = currentTime;
            frames = 0;
        }

        ImGui::End();

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

    saveUniformsToFile(uniformFileName);

    sndPlaySound(0, 0);
    window_end(info);

    return 0;
}