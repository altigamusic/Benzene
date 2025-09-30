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
#include "CameraController.h"
#include "uniform.h"
#include <regex>
#include "imgui/imgui_benzene_widgets.h"

int sidebarWidth;
int sidebarHeight;
int windowWidth = 1200;
int windowHeight = 800;
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
static GLuint windowOffsetLocation = -1;

static GLuint cameraPositionLocation = -1;
static GLuint cameraTargetLocation = -1;

std::string currentShader;
std::vector<Uniform> uniformList;

void openDebugWindow(std::string error)
{
    debugError += error + "\n";
    showDebugWindow = true;
}

void closeDebugWindow()
{
    debugError = "";
    showDebugWindow = false;
}

std::vector<std::string> getUndeclaredIdentifiers(std::string error)
{
    std::regex undeclaredIdentifierRegex(
        "Undeclared identifier:\\s*(\\w+)\\s*$|['\"](\\w+)['\"]\\s*:\\s*undeclared identifier", std::regex::icase);

    std::vector<std::string> result;
    std::smatch match;

    std::string::const_iterator searchStart(error.cbegin());
    while (std::regex_search(searchStart, error.cend(), match, undeclaredIdentifierRegex))
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
    std::ofstream file(filename);
    if (!file.is_open())
    {
        openDebugWindow("Failed to open file for saving uniforms: " + filename);
        return;
    }

    for (const Uniform& uniform : uniformList)
    {
        file << uniform.name;

        switch (uniform.type)
        {
        case UniformType::Float:
            file << ";float;";
            break;
        case UniformType::Int:
            file << ";int;";
            break;
        case UniformType::Bool:
            file << ";bool;";
            break;
        case UniformType::Vec2:
            file << ";vec2;";
            break;
        case UniformType::Vec3:
            file << ";vec3;";
            break;
        case UniformType::Color:
            file << ";color;";
            break;
        case UniformType::Vec4:
            file << ";vec4;";
            break;
        }

        for (auto& keyframe : uniform.keyframes)
        {
            file << (int)keyframe.time << ",";

            switch (uniform.type)
            {
            case UniformType::Float:
                file << std::fixed << std::setprecision(6) << keyframe.value.f;
                break;
            case UniformType::Int:
                file << keyframe.value.i;
                break;
            case UniformType::Bool:
                file << (keyframe.value.b ? "true" : "false");
                break;
            case UniformType::Vec2:
                file << std::fixed << std::setprecision(6) << keyframe.value.v2[0] << "/" << keyframe.value.v2[1];
                break;
            case UniformType::Vec3:
                file << keyframe.value.v3[0] << "/" << keyframe.value.v3[1] << "/" << keyframe.value.v3[2];
                break;
            case UniformType::Color:
                file << keyframe.value.v3[0] << "/" << keyframe.value.v3[1] << "/" << keyframe.value.v3[2];
                break;
            case UniformType::Vec4:
                file << keyframe.value.v4[0] << "/" << keyframe.value.v4[1] << "/" << keyframe.value.v4[2] << "/" << keyframe.value.v4[3];
                break;
            }

            file << "," << keyframe.interpolationToNumber() << "," << keyframe.interpolationFactor << ";";
        }

        file << "\n";
    }

    file.close();
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
        "uniform vec2 _res, _windowOffset;\n"
        "uniform float _t;\n"
        "uniform vec3 _cp, _ct;\n"
        "vec2 fragCoord = gl_FragCoord.xy - _windowOffset;\n" +
        generateUniformCode() + fragmentShaderSource;

    const char* srcPtr = source.c_str();
    fragmentShaderProgram = glCreateShaderProgramv(GL_FRAGMENT_SHADER, 1, &srcPtr);

    glGetProgramInfoLog(fragmentShaderProgram, 500, &length, infoLog);

    std::string error(infoLog, length);

    if (length > 0)
    {
        if (!didTryInjecting)
        {
            auto undeclaredIdentifiers = getUndeclaredIdentifiers(error);

            // Inject all undeclared identifiers as uniforms and recompile
            for (const std::string& name : undeclaredIdentifiers)
            {
                uniformList.push_back(Uniform(name, UniformType::Float));
            }

            return loadFragmentShader(fragmentShaderSource, true);
        }

        openDebugWindow(error);
        return;
    }
    else
    {
        closeDebugWindow();
    }

    timeLocation = glGetUniformLocation(fragmentShaderProgram, "_t");
    resolutionLocation = glGetUniformLocation(fragmentShaderProgram, "_res");
    windowOffsetLocation = glGetUniformLocation(fragmentShaderProgram, "_windowOffset");
    cameraPositionLocation = glGetUniformLocation(fragmentShaderProgram, "_cp");
    cameraTargetLocation = glGetUniformLocation(fragmentShaderProgram, "_ct");

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
    initIntro(fragmentShaderProgram);
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

void updateUniforms(long timeInMs)
{
    const float ftime = 0.001f * (float)timeInMs;

    glUniform1f(timeLocation, ftime);
    glUniform2f(resolutionLocation, viewportWidth, viewportHeight);
    glUniform2f(windowOffsetLocation, sidebarWidth, timelineHeight);

    glUniform3f(cameraPositionLocation, cameraController.position.x, cameraController.position.y, cameraController.position.z);
    glUniform3f(cameraTargetLocation, cameraController.target.x, cameraController.target.y, cameraController.target.z);

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

bool renderAndUpdateUniforms(float time, bool& shouldKeepPlaying)
{
    if (uniformList.empty()) return false;

    bool didAnythingChange = false;

    ImGui::SeparatorText("Uniforms");
    ImGui::BeginChild("Uniforms", ImVec2(0, sidebarHeight / 2));

    auto uniformToDelete = uniformList.end();

    for (auto uniformIt = uniformList.begin(); uniformIt != uniformList.end(); ++uniformIt)
    {
        Uniform& uniform = *uniformIt;

        UniformValue value = uniform.valueAtTime(time);
        bool didThisUniformChange = false;
        UniformKeyframe* keyframeAtCurrentTime = uniform.getKeyframeAtTime(time);

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

        int uniformTypeIndex = uniform.type == UniformType::Float   ? 0
                               : uniform.type == UniformType::Vec2  ? 1
                               : uniform.type == UniformType::Vec3  ? 2
                               : uniform.type == UniformType::Color ? 3
                                                                    : 0;

        char* items[] = {"float", "vec2", "vec3", "color"};

        if (ImGui::BeginPopupContextItem(uniform.name.c_str()))
        {
            if (ImGui::Combo("Type", &uniformTypeIndex, items, 4))
            {
                switch (uniformTypeIndex)
                {
                case 0:
                    uniform.type = UniformType::Float;
                    break;
                case 1:
                    uniform.type = UniformType::Vec2;
                    break;
                case 2:
                    uniform.type = UniformType::Vec3;
                    break;
                case 3:
                    uniform.type = UniformType::Color;
                    break;
                default:
                    break;
                }

                // Don't activate didThisUniformChange here so a keyframe won't be created
                didAnythingChange = true;
                uniform.keyframes.clear();
            }

            if (ImGui::Selectable("Delete"))
            {
                uniformToDelete = uniformIt;
                ImGui::End();
                continue;
            }

            ImGui::End();
        }

        bool hasKeyframeAtCurrentTime = keyframeAtCurrentTime != nullptr;
        bool shouldHaveKeyframeAtCurrentTime = hasKeyframeAtCurrentTime;
        KeyframeInterpolation interpolation =
            hasKeyframeAtCurrentTime ? keyframeAtCurrentTime->interpolation : KeyframeInterpolation::Linear;
        float tension = hasKeyframeAtCurrentTime ? keyframeAtCurrentTime->interpolationFactor : 0.5f;

        ImGui::SameLine();
        bool didKeyframeInfoChange =
            KeyframeMarker((uniform.name + "_kf").c_str(), &shouldHaveKeyframeAtCurrentTime, &interpolation, &tension);

        float lastKeyframeTime = uniform.keyframes.empty() ? 0 : uniform.keyframes.back().time;
        bool isBeyondLastKeyframe = lastKeyframeTime <= time;

        // Set a keyframe if the uniform changed *only if* it's before another keyframe!
        // This is because if it's after the last one, it's more natural to just update the last keyframe value instead.
        // However, if we're between two keyframes, we don't know which keyframe the user would want to change, or how to interpolate the
        // data.
        bool shouldSetKeyframeDueToUniformChange = didThisUniformChange && !isBeyondLastKeyframe;
        bool shouldSetKeyframeDueToMarkerChange = didKeyframeInfoChange && shouldHaveKeyframeAtCurrentTime;

        bool shouldSetKeyframe = shouldSetKeyframeDueToUniformChange || shouldSetKeyframeDueToMarkerChange;
        bool shouldRemoveKeyframe = didKeyframeInfoChange && !shouldHaveKeyframeAtCurrentTime && hasKeyframeAtCurrentTime;
        bool shouldUpdateLastKeyframeValue = didThisUniformChange && isBeyondLastKeyframe;

        if (shouldSetKeyframe)
        {
            uniform.setKeyframeAtTime(time, value, interpolation, tension);
            didAnythingChange = true;
            shouldKeepPlaying = false; // Pause only if a keyframe was created, no other reason
        }
        else if (shouldUpdateLastKeyframeValue)
        {
            uniform.setKeyframeAtTime(lastKeyframeTime, value, interpolation, tension);
            didAnythingChange = true;
        }
        else if (shouldRemoveKeyframe)
        {
            // Remove the keyframe at the current time
            uniform.removeKeyframeAtTime(time);
            didAnythingChange = true;
        }
    }

    if (uniformToDelete != uniformList.end())
    {
        uniformList.erase(uniformToDelete);
    }

    ImGui::EndChild();

    return didAnythingChange;
}

UniformType getUniformTypeFromString(const std::string& typeString)
{
    if (typeString == "float")
        return UniformType::Float;
    else if (typeString == "int")
        return UniformType::Int;
    else if (typeString == "bool")
        return UniformType::Bool;
    else if (typeString == "vec2")
        return UniformType::Vec2;
    else if (typeString == "vec3")
        return UniformType::Vec3;
    else if (typeString == "color")
        return UniformType::Color;
    else if (typeString == "vec4")
        return UniformType::Vec4;
    else
        return UniformType::Untyped;
}

UniformValue getUniformValueFromString(const UniformType type, const std::string& valueStr)
{
    UniformValue value{};
    bool loadError = false;

    switch (type)
    {
    case UniformType::Float:
        value.f = std::stof(valueStr);
        break;
    case UniformType::Int:
        value.i = std::stoi(valueStr);
        break;
    case UniformType::Bool:
        value.b = (valueStr == "true");
        loadError = valueStr != "false" && valueStr != "true";
        break;
    case UniformType::Vec2:
        loadError = sscanf(valueStr.c_str(), "%f/%f", &value.v2[0], &value.v2[1]) != 2;
        break;
    case UniformType::Vec3:
        loadError = sscanf(valueStr.c_str(), "%f/%f/%f", &value.v3[0], &value.v3[1], &value.v3[2]) != 3;
        break;
    case UniformType::Color:
        loadError = sscanf(valueStr.c_str(), "%f/%f/%f", &value.v3[0], &value.v3[1], &value.v3[2]) != 3;
        break;
    case UniformType::Vec4:
        loadError = sscanf(valueStr.c_str(), "%f/%f/%f/%f", &value.v4[0], &value.v4[1], &value.v4[2], &value.v4[3]) != 4;
        break;
    default:
        loadError = true;
        break;
    }

    if (loadError)
    {
        throw std::runtime_error("Failed to parse uniform value from string: " + valueStr);
    }

    return value;
}

Uniform loadUniformFromLine(std::stringstream& line)
{
    std::string name, typeStr, valueStr = "";
    bool loadError = false;

    if (!std::getline(line, name, ';') || !std::getline(line, typeStr, ';'))
        throw new std::runtime_error("Failed to read uniform from file");

    std::getline(line, valueStr);

    Uniform uniform(name);
    uniform.type = getUniformTypeFromString(typeStr);

    std::stringstream keyframeStream(valueStr);
    std::string keyframeStr;

    while (std::getline(keyframeStream, keyframeStr, ';'))
    {
        if (keyframeStr.empty()) continue;

        std::stringstream keyframeStream(keyframeStr);
        std::string timeStr, valueStr, interpolationTypeStr, interpolationFactorStr;

        if (!std::getline(keyframeStream, timeStr, ',')) throw new std::runtime_error("Invalid keyframe time");
        if (!std::getline(keyframeStream, valueStr, ',')) throw new std::runtime_error("Invalid keyframe value");
        if (!std::getline(keyframeStream, interpolationTypeStr, ',')) throw new std::runtime_error("Invalid keyframe interpolation type");
        if (!std::getline(keyframeStream, interpolationFactorStr, ','))
            throw new std::runtime_error("Invalid keyframe interpolation factor");

        float time = std::stof(timeStr);
        UniformValue value = getUniformValueFromString(uniform.type, valueStr);
        KeyframeInterpolation interpolation = static_cast<KeyframeInterpolation>(std::stoi(interpolationTypeStr));
        float interpolationFactor = std::stof(interpolationFactorStr);

        uniform.setKeyframeAtTime(time, value, interpolation, interpolationFactor);
    }

    return uniform;
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
        std::stringstream lineStream(line);
        std::string name, type, valueStr;
        bool loadError = false;

        try
        {
            Uniform nextUniform = loadUniformFromLine(lineStream);
            if (nextUniform.type == UniformType::Untyped) throw std::runtime_error("Failed to load uniform from line: " + line);
            uniformList.push_back(nextUniform);
        }
        catch (const std::exception& e)
        {
            openDebugWindow(e.what());
        }
    }

    file.close();
}

std::vector<int> getAllKeyframes()
{
    std::vector<int> keyframes;

    for (const Uniform& uniform : uniformList)
    {
        for (const UniformKeyframe& keyframe : uniform.keyframes)
        {
            int time = static_cast<int>(keyframe.time);
            if (std::find(keyframes.begin(), keyframes.end(), time) == keyframes.end())
            {
                keyframes.push_back(time);
            }
        }
    }

    std::sort(keyframes.begin(), keyframes.end());

    return keyframes;
}

bool renderTimelines(int* time, int maxTime)
{
    bool didChange = false;

    didChange |= TimeSlider("Time", time, 0, maxTime);

    for (const Uniform& uniform : uniformList)
    {
        std::vector<int> keyframes;

        for (const UniformKeyframe& keyframe : uniform.keyframes)
            keyframes.push_back(static_cast<int>(keyframe.time));

        didChange |= KeyframeSlider(uniform.name.c_str(), time, 0, maxTime, keyframes);
    }

    return didChange;
}

int findPreviousKeyframe(int t, const std::vector<int>& keyframes)
{
    auto it = std::lower_bound(keyframes.begin(), keyframes.end(), t);

    if (it != keyframes.begin())
    {
        --it;
        return *it;
    }

    return -1; // No previous keyframe
}

int findNextKeyframe(int t, const std::vector<int>& keyframes)
{
    auto it = std::upper_bound(keyframes.begin(), keyframes.end(), t);

    if (it != keyframes.end()) return *it;

    return -1; // No next keyframe
}

bool handleKeyScrubbing(int& t, int maxTimelineTime)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard) return false;

    // Get sorted list of all keyframes
    std::vector<int> keyframes = getAllKeyframes();

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))
    {
        if (!io.KeyCtrl)
        {
            t = max(0, t - 100);
            return true;
        }

        int prevKeyframe = findPreviousKeyframe(t, keyframes);

        if (prevKeyframe != -1)
        {
            t = prevKeyframe;
            return true;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow))
    {
        if (!io.KeyCtrl)
        {
            t = min(maxTimelineTime, t + 100);
            return true;
        }

        int nextKeyframe = findNextKeyframe(t, keyframes);

        if (nextKeyframe != -1)
        {
            t = nextKeyframe;
            return true;
        }
    }

    return false;
}

bool renderMenuBar()
{
    bool didChange = false;

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("Reload Uniforms From File"))
            {
                loadUniformsFromFile(uniformFileName);
                loadFragmentShader(currentShader.c_str());
                didChange = true;
            }
            if (ImGui::MenuItem("Save Uniforms"))
            {
                saveUniformsToFile(uniformFileName);
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    return didChange;
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

        if (handleKeyScrubbing(t, 140000))
        {
            isPlaying = false;
            shouldRerender = true;
        }

        ImGui::SetNextWindowPos(ImVec2(0, viewportHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(timelineWidth, timelineHeight), ImGuiCond_Always);

        ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        if (ImGui::Button(isPlaying ? "Pause" : "Play"))
        {
            isPlaying = !isPlaying;
            shouldRerender = true;
        }

        if (renderTimelines(&t, 140000))
        {
            shouldRerender = true;
            frames = -1;
            isPlaying = false; // Pause when scrubbing
        }

        ImGui::SetItemTooltip("Space");

        ImGui::End();

        // Sidebar
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(sidebarWidth, sidebarHeight), ImGuiCond_Always);

        ImGui::Begin("Editor", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar |
                ImGuiWindowFlags_NoTitleBar);

        if (renderMenuBar()) shouldRerender = true;

        if (renderAndUpdateUniforms(t, isPlaying)) // The function will pause if necessary
            shouldRerender = true;

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
            if (!showDebugWindow) closeDebugWindow();
        }

        // If the render stopped just now, render to the back buffer
        if (prevShouldRerender && !shouldRerender)
        {
            introLoop(t);
            SwapBuffers(info->hDC);
            introLoop(t);
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
