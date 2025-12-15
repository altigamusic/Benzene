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
#include "CameraKeyframeController.h"
#include "uniform.h"
#include <regex>
#include <set>
#include "imgui/imgui_benzene_widgets.h"
#include "config.h"

int sidebarWidth;
int sidebarHeight;
int windowWidth = 1200;
int windowHeight = 800;
int viewportWidth = 800;
int viewportHeight = 600;
int timelineWidth;
int timelineHeight = 200;
CameraKeyframeController cameraController;
const char* configFileName = "config.json";

int releaseResolutionX;
int releaseResolutionY;

bool showDemoWindow;

int demoTimeLength = 140;
int bpm = 120;

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
bool showSettingsWindow = false;

bool isPlaying = true;
bool isSpaceDown;

static GLuint fragmentShaderProgram = 0;
static GLuint timeLocation;
static GLuint resolutionLocation = -1;
static GLuint windowOffsetLocation = -1;

static GLuint cameraPositionLocation = -1;
static GLuint cameraTargetLocation = -1;
static GLuint cameraRotationLocation = -1;

std::string currentShader;
std::vector<Uniform> uniformList;

std::vector<std::string> groups;
std::string currentGroup;

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
        "Undeclared identifier:\\s*(\\w+)\\s*$|['\"](\\w+)['\"]\\s*:\\s*undeclared "
        "identifier|[uU]ndefined\\s+[Vv]ariable\\s+['\"](\\w+)['\"]",
        std::regex::icase);

    std::vector<std::string> result;
    std::smatch match;

    std::string::const_iterator searchStart(error.cbegin());
    while (std::regex_search(searchStart, error.cend(), match, undeclaredIdentifierRegex))
    {
        if (match.size() > 1)
        {
            // Only one group will match something and the other(s) will be empty
            for (size_t i = 1; i < match.size(); ++i)
            {
                if (match[i].matched)
                {
                    result.push_back(match[i].str());
                    break;
                }
            }
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
        "uniform vec3 _cp, _ct, _cr;\n"
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
                Uniform new_uniform{name, UniformType::Float};
                new_uniform.group = currentGroup;
                uniformList.push_back(new_uniform);
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
    cameraRotationLocation = glGetUniformLocation(fragmentShaderProgram, "_cr");

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

void updateUniforms(const float ftime)
{
    glUniform1f(timeLocation, ftime);
    glUniform2f(resolutionLocation, viewportWidth, viewportHeight);
    glUniform2f(windowOffsetLocation, sidebarWidth, timelineHeight);

    vec3 cp = cameraController.getPosition();
    vec3 ct = cameraController.getTarget();
    vec3 cr = cameraController.getRotation();

    glUniform3f(cameraPositionLocation, cp.x, cp.y, cp.z);
    glUniform3f(cameraTargetLocation, ct.x, ct.y, ct.z);
    glUniform3f(cameraRotationLocation, cr.x, cr.y, cr.z);

    for (Uniform& uniform : uniformList)
    {
        auto value = uniform.valueAtTime(ftime);

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

bool renderSingleUniformTab(std::string group, float time, bool& shouldKeepPlaying)
{
    bool didAnythingChange = false;
    bool shouldReloadFragmentShader = false;

    auto uniformToDelete = uniformList.end();

    for (auto uniformIt = uniformList.begin(); uniformIt != uniformList.end(); ++uniformIt)
    {
        Uniform& uniform = *uniformIt;

        if (uniform.group != group) continue;

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
            didThisUniformChange = ImGui::ColorEdit3(uniform.name.c_str(), value.v3);
            break;
        }

        int uniformTypeIndex = uniform.type == UniformType::Float   ? 0
                               : uniform.type == UniformType::Vec2  ? 1
                               : uniform.type == UniformType::Vec3  ? 2
                               : uniform.type == UniformType::Color ? 3
                                                                    : 0;

        char* items[] = {"float", "vec2", "color"};

        if (ImGui::BeginPopupContextItem(uniform.name.c_str()))
        {
            if (ImGui::Combo("Type", &uniformTypeIndex, items, 3))
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
                    uniform.type = UniformType::Color;
                    break;
                default:
                    break;
                }

                // Don't activate didThisUniformChange here so a keyframe won't be created
                didAnythingChange = true;
                shouldReloadFragmentShader = true;
                uniform.keyframes.clear();
            }

            int uniformGroupIndex = uniform.group.empty() ? 0 : std::find(groups.begin(), groups.end(), uniform.group) - groups.begin();

            // Combo takes an array, so convert the groups to a const char*[]
            std::vector<const char*> groupItems;
            groupItems.reserve(groups.size() + 1);
            groupItems.push_back("(no group)");
            for (const std::string& g : groups) groupItems.push_back(g.c_str());

            if (ImGui::Combo("Group", &uniformGroupIndex, groupItems.data(), groupItems.size()))
            {
                if (uniformGroupIndex == 0) // No group
                    uniform.group.clear();
                else
                    uniform.group = groups[uniformGroupIndex - 1];

                didAnythingChange = true;
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
        shouldReloadFragmentShader = true;
    }

    if (shouldReloadFragmentShader) loadFragmentShader(currentShader.c_str());

    return didAnythingChange;
}

bool renderAndUpdateUniforms(float time, bool& shouldKeepPlaying)
{
    if (uniformList.empty()) return false;

    bool didAnythingChange = false;

    ImGui::SeparatorText("Uniforms");

    // Force the uniform window to leave enough room for the camera panel
    float maxWindowHeight = sidebarHeight - ImGui::GetTextLineHeightWithSpacing() * 20.0f;
    float windowHeight = min(sidebarHeight / 2, maxWindowHeight);

    ImGui::BeginChild("Uniforms", ImVec2(0, windowHeight));

    ImGui::BeginTabBar("Uniforms");

    if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip))
    {
        groups.push_back("Group " + std::to_string(groups.size() + 1));
    }

    static int currentlyRenamedGroup = -1;

    if (ImGui::BeginTabItem("Unsorted"))
    {
        currentGroup = "";
        didAnythingChange = renderSingleUniformTab("", time, shouldKeepPlaying);
        ImGui::EndTabItem();
    }

    for (int i = 0; i < groups.size(); i++)
    {
        std::string group = groups[i];

        if (ImGui::BeginTabItem(group.c_str()))
        {
            currentGroup = group;
            didAnythingChange = renderSingleUniformTab(group, time, shouldKeepPlaying);

            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::Selectable("Rename"))
                {
                    currentlyRenamedGroup = i;
                    ImGui::CloseCurrentPopup();
                    ImGui::OpenPopup("Rename Group");
                }

                ImGui::EndPopup();
            }
            ImGui::EndTabItem();
        }
    }

    if (currentlyRenamedGroup >= 0)
    {
        ImGui::Begin("Rename Group", NULL, ImGuiWindowFlags_AlwaysAutoResize);

        char newGroupName[128] = {0};
        std::strncpy(newGroupName, groups[currentlyRenamedGroup].c_str(), 127);
        if (ImGui::InputText("New Name", newGroupName, 128))
        {
            // Rename the group and uniforms
            groups[currentlyRenamedGroup] = newGroupName;

            for (Uniform& uniform : uniformList)
            {
                if (uniform.group == groups[currentlyRenamedGroup])
                {
                    uniform.group = newGroupName;
                }
            }
        }

        if (ImGui::Button("OK"))
        {
            currentlyRenamedGroup = -1;
        }

        ImGui::End();
    }

    ImGui::EndTabBar();

    ImGui::EndChild();

    return didAnythingChange;
}

void loadConfigFromFile(const std::string& filename)
{
    try
    {
        UniformConfig cfg = loadConfig(filename);
        uniformList = std::move(cfg.uniformList);
        bpm = cfg.bpm;
        releaseResolutionX = cfg.resolutionX;
        releaseResolutionY = cfg.resolutionY;
        demoTimeLength = cfg.lengthInBeats;
        if (cfg.cameraPosition.has_value()) cameraController.positionUniform = cfg.cameraPosition.value();
        if (cfg.cameraRotation.has_value()) cameraController.rotationUniform = cfg.cameraRotation.value();

        // Load groups
        std::set<std::string> groupSet;
        for (const Uniform& u : uniformList)
        {
            if (!u.group.empty()) groupSet.insert(u.group);
        }
        groups = std::vector<std::string>(groupSet.begin(), groupSet.end());
    }
    catch (const std::exception& e)
    {
        openDebugWindow(e.what());
    }
}

void saveConfigToFile(const std::string& filename)
{
    try
    {
        UniformConfig config{(float)bpm, (float)demoTimeLength, releaseResolutionX, releaseResolutionY, uniformList,
            cameraController.positionUniform, cameraController.rotationUniform};
        saveConfig(config, filename);
    }
    catch (const std::exception& e)
    {
        openDebugWindow(e.what());
    }
}

std::vector<int> getAllKeyframes()
{
    std::vector<int> keyframes;

    auto appendKeyframes = [&keyframes](const Uniform& uniform)
    {
        for (const UniformKeyframe& keyframe : uniform.keyframes)
        {
            int time = static_cast<int>(keyframe.time);
            if (std::find(keyframes.begin(), keyframes.end(), time) == keyframes.end())
            {
                keyframes.push_back(time);
            }
        }
    };

    for (const Uniform& uniform : uniformList)
    {
        appendKeyframes(uniform);
    }

    appendKeyframes(cameraController.positionUniform);
    appendKeyframes(cameraController.rotationUniform);

    std::sort(keyframes.begin(), keyframes.end());

    return keyframes;
}

bool renderTimelines(float* time, float& minTime, float& maxTime)
{
    bool didChange = false;

    ZoomPanSlider("Zoom", &minTime, &maxTime, 0.0f, demoTimeLength);
    didChange |= TimeSlider("Time", time, minTime, maxTime);

    if (cameraController.positionUniform.keyframes.size() > 1)
    {
        // Camera slider is special because it's controlled differently
        // Camera position and camera target have the same keyframes
        std::vector<float> keyframes;

        for (const UniformKeyframe& keyframe : cameraController.positionUniform.keyframes)
            keyframes.push_back(keyframe.time);

        KeyframeMovementData kfMovement;
        if (KeyframeSlider("Camera", time, minTime, maxTime, keyframes, &kfMovement))
        {
            if (kfMovement.index >= 0)
            {
                float minValue = kfMovement.index == 0 ? 0 : (keyframes[kfMovement.index - 1] + 1);
                float maxValue = kfMovement.index == keyframes.size() - 1 ? maxTime : (keyframes[kfMovement.index + 1] - 1);
                // Round to snap keyframes
                float newTime = std::round(std::clamp(kfMovement.newTime, minValue, maxValue));

                cameraController.positionUniform.keyframes[kfMovement.index].time = newTime;
                cameraController.rotationUniform.keyframes[kfMovement.index].time = newTime;
            }

            didChange = true;
        }
    }

    for (Uniform& uniform : uniformList)
    {
        // Display timelines only for animated uniforms, i.e. uniforms with 2+ keyframes
        if (uniform.keyframes.size() <= 1 || uniform.group != currentGroup) continue;

        std::vector<float> keyframes;

        for (const UniformKeyframe& keyframe : uniform.keyframes)
            keyframes.push_back(keyframe.time);

        KeyframeMovementData kfMovement;

        if (KeyframeSlider(uniform.name.c_str(), time, minTime, maxTime, keyframes, &kfMovement))
        {
            if (kfMovement.index >= 0)
            {
                // Move the keyframe - prevent overlapping by restricting the bounds
                float minValue = kfMovement.index == 0 ? 0 : (keyframes[kfMovement.index - 1] + 1);
                float maxValue = kfMovement.index == keyframes.size() - 1 ? maxTime : (keyframes[kfMovement.index + 1] - 1);

                // Round to snap keyframes
                uniform.keyframes[kfMovement.index].time = std::round(std::clamp(kfMovement.newTime, minValue, maxValue));
            }

            didChange = true;
        }
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

bool handleKeyScrubbing(float& t, int maxTimelineTime, bool backButton, bool forwardButton)
{
    ImGuiIO& io = ImGui::GetIO();
    if (io.WantCaptureKeyboard && !backButton && !forwardButton) return false;

    // Get sorted list of all keyframes
    std::vector<int> keyframes = getAllKeyframes();

    if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || backButton)
    {
        if (io.KeyCtrl || backButton)
        {
            int prevKeyframe = findPreviousKeyframe(t, keyframes);

            if (prevKeyframe != -1)
            {
                t = prevKeyframe;
                return true;
            }
        }
        else
        {
            t = max(0, std::ceil(t) - 1);
            return true;
        }
    }

    if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) || forwardButton)
    {
        if (io.KeyCtrl || forwardButton)
        {
            int nextKeyframe = findNextKeyframe(t, keyframes);

            if (nextKeyframe != -1)
            {
                t = nextKeyframe;
                return true;
            }
        }
        else
        {
            t = min(maxTimelineTime, std::floor(t) + 1);
            return true;
        }
    }

    return false;
}

void renderDebugWindow()
{
    if (ImGui::Begin("Shader Debug", &showDebugWindow))
    {
        ImGui::TextUnformatted(debugError.c_str());
        ImGui::End();
        if (!showDebugWindow) closeDebugWindow();
    }
}

void renderSettingsWindow()
{
    if (!ImGui::Begin("Settings", &showSettingsWindow, ImGuiWindowFlags_NoCollapse)) return;

    bool didResolutionChange = false;

    ImGui::Text("Resolution: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    didResolutionChange |= ImGui::InputInt(" x ##xres", &releaseResolutionX, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    didResolutionChange |= ImGui::InputInt("##yres", &releaseResolutionY, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);

    if (didResolutionChange) resizeWindow(windowWidth, windowHeight);

    ImGui::End();
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
                loadConfigFromFile(configFileName);
                loadFragmentShader(currentShader.c_str());
                didChange = true;
            }
            if (ImGui::MenuItem("Save Uniforms"))
            {
                saveConfigToFile(configFileName);
            }
            if (ImGui::MenuItem("Settings"))
            {
                showSettingsWindow = true;
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    return didChange;
}

float msToBeats(long ms, float bpm) { return ms * bpm / 60000; }
long beatsToMs(float beats, float bpm) { return 60000 / bpm * beats; }

int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    bool showSaveDialog = false;
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
    loadConfigFromFile(configFileName);

    float timelineViewStart = 0.0f;
    float timelineViewEnd = demoTimeLength;

    long prevSystemTime = timeGetTime();
    long playStartSystemTime = prevSystemTime;
    long fpsStartSystemTime = prevSystemTime;
    long currentSystemTime = 0;

    float t = 0;             // The current time, in beats
    float playStartTime = 0; // The time at which playback started, in beats
    int frames = 0;

    bool prevShouldRerender = false;

    while (!done)
    {
        currentSystemTime = timeGetTime();

        long timeDeltaMs = currentSystemTime - prevSystemTime;
        prevSystemTime = currentSystemTime;

        if (playStartSystemTime > 0)
        {
            long playTimeMs = currentSystemTime - playStartSystemTime;
            float playTimeBeats = msToBeats(playTimeMs, bpm);
            t = playStartTime + playTimeBeats;
            t = fmodf(t, demoTimeLength);
        }

        cameraController.startFrame(t);

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) showSaveDialog = true;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);
        if (showSettingsWindow) renderSettingsWindow();
        if (showDebugWindow) renderDebugWindow();

        // Move camera by keyboard input
        cameraController.updateCamera(timeDeltaMs);

        bool shouldRerender = isPlaying;

        ImGui::SetNextWindowPos(ImVec2(0, viewportHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(timelineWidth, timelineHeight), ImGuiCond_Always);

        ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        bool backButton = ImGui::Button("Prev");
        ImGui::SameLine();
        bool forwardButton = ImGui::Button("Next");
        ImGui::SameLine();

        if (PlayPauseButton(isPlaying))
        {
            isPlaying = !isPlaying;
            shouldRerender = true;
        }

        if (handleKeyScrubbing(t, demoTimeLength, backButton, forwardButton))
        {
            isPlaying = false;
            shouldRerender = true;
        }

        ImGui::SameLine(0, 50);
        ImGui::Text("BPM:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::DragInt("##BPM", &bpm, 1.0f, 10, 5000);

        ImGui::SameLine(0, 50);
        ImGui::Text("Length:");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(50);
        ImGui::DragInt("beats", &demoTimeLength, 1.0f, 0, INT_MAX);

        if (renderTimelines(&t, timelineViewStart, timelineViewEnd))
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

        int fpsT = currentSystemTime - fpsStartSystemTime;
        ImGui::SetCursorPosY(ImGui::GetWindowHeight() - ImGui::GetTextLineHeightWithSpacing() * 2);
        ImGui::Text("Frame delta: %.3f ms (%.1f FPS)\n", frames == 0 ? 0 : ((float)fpsT / frames), fpsT == 0 ? 0 : frames * 1000.f / fpsT);

        if (fpsT > 2000)
        {
            fpsStartSystemTime = currentSystemTime;
            frames = 0;
        }

        ImGui::End();

        // Try reloading the file
        // TODO: Maybe change the way this is done or something
        shouldRerender |= reloadFragmentShaderFromFile();

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

        // If isPlaying changed, propagate the change to the time variables
        playStartSystemTime = isPlaying ? currentSystemTime : -1;
        playStartTime = isPlaying ? t : -1;

        prevShouldRerender = shouldRerender;

        if (showSaveDialog)
        {
            // Place the save dialog in the center-top
            ImGuiIO& io = ImGui::GetIO();
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.125f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));

            if (ImGui::Begin("Save?", &showSaveDialog, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse))
            {
                ImGui::Text("Save changes before exiting?");
                if (ImGui::Button("Yes"))
                {
                    saveConfigToFile(configFileName);
                    showSaveDialog = false;
                    done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("No"))
                {
                    showSaveDialog = false;
                    done = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel"))
                {
                    showSaveDialog = false;
                }

                ImGui::End();
            }
        }

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
