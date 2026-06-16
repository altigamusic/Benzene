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
#include "editor/CameraController.h"
#include "editor/CameraKeyframeController.h"
#include "editor/InputState.h"
#include "editor/editor_music.h"
#include "editor/uniform.h"
#include <regex>
#include <set>
#include "imgui/imgui_benzene_widgets.h"
#include "editor/config.h"
#include "editor/components/keyframe_marker.h"
#include "editor/window_renderer.h"
#include "editor/components/debug_window.h"
#include "editor/components/save_dialog.h"
#include "editor/uniform_editor.h"
#include "editor/timeline.h"
#include "editor/EditorState.h"
#include "editor/ActionHistory.h"

EditorState editorState;
ActionHistory actionHistory;
const char* configFileName = "config.json";

bool showDemoWindow;

bool isAbLooping = false;
float loopStartTime = 0.0f;
float loopEndTime = 140.0f;
bool shouldPlayMusic = true;

DebugWindow debugWindow;
WindowRenderer windowRenderer(debugWindow);

bool showSettingsWindow = false;

bool isPlaying = true;
bool isEndKeyframe = true;

KeyboardState keyboardState;
MouseState mouseState;

void updateUniforms(const float ftime)
{
    windowRenderer.updateUniforms(
        ftime, editorState.config.uniformList, editorState.cameraController, isEndKeyframe, editorState.config.shaderQuantizationDigits);
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
        windowRenderer.resize(LOWORD(lParam), HIWORD(lParam));
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

    if (!wantCaptureKeyboard) keyboardState.onMessage(uMsg, wParam);
    if (!wantCaptureMouse) mouseState.onMessage(hWnd, uMsg, wParam, lParam);

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
        dmScreenSettings.dmPelsWidth = windowRenderer.windowWidth;
        dmScreenSettings.dmPelsHeight = windowRenderer.windowHeight;
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
    rec.right = windowRenderer.windowWidth;
    rec.bottom = windowRenderer.windowHeight;
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

void loadConfigFromFile(const std::string& filename)
{
    try
    {
        BenzeneConfig cfg = loadConfig(filename);
        if (cfg.musicWavFile.has_value() && !cfg.musicWavFile->empty())
        {
            EditorMusic::LoadWavFile(cfg.musicWavFile->c_str());
        }

        loopEndTime = cfg.lengthInBeats; // Update loop end time when config is loaded

        // Load groups
        std::set<std::string> groupSet;
        for (const Uniform& u : cfg.uniformList)
        {
            if (!u.group.empty()) groupSet.insert(u.group);
        }
        editorState.groups = std::vector<std::string>(groupSet.begin(), groupSet.end());

        editorState.fromConfig(std::move(cfg));
    }
    catch (const std::exception& e)
    {
        debugWindow.open(e.what());
    }
}

void saveConfigToFile(const std::string& filename)
{
    try
    {
        const char* loadedMusicPath = EditorMusic::GetLoadedFilePath();
        BenzeneConfig cfg = editorState.toConfig();
        cfg.musicWavFile =
            (loadedMusicPath != nullptr && loadedMusicPath[0] != '\0') ? std::optional<std::string>(loadedMusicPath) : std::nullopt;
        saveConfig(cfg, filename);
    }
    catch (const std::exception& e)
    {
        debugWindow.open(e.what());
    }
}

void renderSettingsWindow()
{
    if (!ImGui::Begin("Settings", &showSettingsWindow, ImGuiWindowFlags_NoCollapse)) return;

    ImGui::Text("Resolution: ");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    ImGui::InputInt(" x ##xres", &editorState.config.resolutionX, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    ImGui::InputInt("##yres", &editorState.config.resolutionY, 0, 0, ImGuiInputTextFlags_EnterReturnsTrue);

    ImGui::SeparatorText("Shader Quantization");

    ImGui::Text("Default digits:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(60);
    if (ImGui::InputInt("##shaderQuantDefault", &editorState.config.shaderQuantizationDigits, 1, 0, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        if (editorState.config.shaderQuantizationDigits < 0) editorState.config.shaderQuantizationDigits = 0;
    }

    ImGui::End();
}

/// <summary>
/// Render the toolbar, and run button functions if clicked.
/// </summary>
/// <param name="t">The current time, in beats. Modified if the prev/next buttons are clicked.</param>
/// <param name="info">Used for opening a file dialog if the user wants to choose a music file.</param>
static bool renderToolbar(float& t, WININFO* info)
{
    bool shouldRerender = false;

    if (ImGui::Button("Prev") && scrubToPreviousKeyframe(t, isEndKeyframe, editorState.config.uniformList, editorState.cameraController))
    {
        isPlaying = false;
        shouldRerender = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Next") && scrubToNextKeyframe(t, isEndKeyframe, editorState.config.uniformList, editorState.cameraController))
    {
        isPlaying = false;
        shouldRerender = true;
    }

    ImGui::SameLine();
    if (PlayPauseButton(isPlaying))
    {
        isPlaying = !isPlaying;
        shouldRerender = true;
    }

    ImGui::SameLine(0, 50);
    ImGui::Text("BPM:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    int bpmInt = (int)editorState.config.bpm;
    if (ImGui::DragInt("##BPM", &bpmInt, 1.0f, 10, 5000)) editorState.config.bpm = (float)bpmInt;

    ImGui::SameLine(0, 50);
    ImGui::Text("Length:");

    ImGui::SameLine();
    ImGui::SetNextItemWidth(50);
    int demoTimeLength = (int)editorState.config.lengthInBeats;
    int prevDemoTimeLength = demoTimeLength;
    if (ImGui::DragInt("beats", &demoTimeLength, 1.0f, 0, INT_MAX))
    {
        editorState.config.lengthInBeats = (float)demoTimeLength;
        if (loopEndTime > demoTimeLength) loopEndTime = (float)demoTimeLength;
    }

    ImGui::SameLine(0, 50);
    ImGui::Checkbox("Loop Segment", &isAbLooping);
    ImGui::SameLine();
    ImGui::Checkbox("Play Music", &shouldPlayMusic);
    ImGui::SameLine();
    if (ImGui::Button("Open..."))
    {
        EditorMusic::OpenFileDialogAndLoad(info->hWnd);
    }

    return shouldRerender;
}

static bool renderMenuBar()
{
    bool didChange = false;

    if (!ImGui::BeginMenuBar()) return false;

    if (ImGui::BeginMenu("File"))
    {
        if (ImGui::MenuItem("Reload Uniforms From File"))
        {
            loadConfigFromFile(configFileName);
            windowRenderer.reload(editorState.config.uniformList, editorState.currentGroup);
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

    if (ImGui::BeginMenu("Edit"))
    {
        std::optional<std::string> undoString = actionHistory.describeUndo();
        std::optional<std::string> redoString = actionHistory.describeRedo();

        if (!undoString.has_value())
        {
            ImGui::MenuItem("Undo", "Ctrl-Z", false, false);
        }
        else if (ImGui::MenuItem(("Undo " + undoString.value()).c_str(), "Ctrl-Z"))
        {
            actionHistory.undo(editorState);
        }

        if (!redoString.has_value())
        {
            ImGui::MenuItem("Redo", "Ctrl-Y", false, false);
        }
        else if (ImGui::MenuItem(("Redo " + redoString.value()).c_str(), "Ctrl-Y"))
        {
            actionHistory.redo(editorState);
        }

        ImGui::EndMenu();
    }

    ImGui::EndMenuBar();

    return didChange;
}

/// <summary>
/// Handle keyboard shortcuts.
/// </summary>
/// <param name="keyboard">The keyboard state.</param>
/// <param name="t">The current time, in beats. Modified if a shortcut that changes time is pressed.</param>
/// <returns>True if the screen should be rerendered, false otherwise.</returns>
static bool handleKeyboard(const KeyboardState& keyboard, float& t)
{
    bool shouldRerender = false;

    if (keyboard.wasKeyPressed(VK_ESCAPE)) PostQuitMessage(0);

    if (keyboard.wasKeyPressed(VK_F1)) showDemoWindow = true;

    if (keyboard.wasKeyPressed(VK_SPACE)) isPlaying = !isPlaying;

    if (keyboard.isDown(VK_CONTROL) && keyboard.wasKeyPressed('Z')) actionHistory.undo(editorState);

    if (keyboard.isDown(VK_CONTROL) && keyboard.wasKeyPressed('Y')) actionHistory.redo(editorState);

    if (handleKeyScrubbing(keyboard, t, isEndKeyframe, (int)editorState.config.lengthInBeats, editorState.config.uniformList,
            editorState.cameraController))
    {
        isPlaying = false;
        shouldRerender = true;
    }

    return shouldRerender;
}

float msToBeats(long ms, float bpm) { return ms * bpm / 60000; }
long beatsToMs(float beats, float bpm) { return 60000 / bpm * beats; }

int WINAPI WinMain(HINSTANCE instance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    MSG msg;
    SaveDialog saveDialog;
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

    editorState.cameraController.recalculateCameraTarget();
    loadConfigFromFile(configFileName);

    float timelineViewStart = 0.0f;
    float timelineViewEnd = editorState.config.lengthInBeats;

    long long prevSystemTime = timeGetTime();
    long long playStartSystemTime = prevSystemTime;
    long long fpsStartSystemTime = prevSystemTime;
    long long currentSystemTime = 0;

    float timeInBeats = 0;   // The current time, in beats
    float playStartTime = 0; // The time at which playback started, in beats
    int frames = 0;

    bool prevShouldRerender = false;

    while (!done)
    {
        currentSystemTime = (unsigned long)timeGetTime();

        long timeDeltaMs = currentSystemTime - prevSystemTime;
        prevSystemTime = currentSystemTime;

        if (playStartSystemTime > 0)
        {
            long playTimeMs = currentSystemTime - playStartSystemTime;
            float playTimeBeats = msToBeats(playTimeMs, editorState.config.bpm);
            timeInBeats = playStartTime + playTimeBeats;

            if (isAbLooping && loopEndTime > loopStartTime)
                // Note: fmodf doesn't work on negative values, so if the player is before the loop, it'll play until it.
                // This is a feature - it's more convenient and preferable.

                timeInBeats = fmodf(timeInBeats - loopStartTime, loopEndTime - loopStartTime) + loopStartTime;
            else
                timeInBeats = fmodf(timeInBeats, editorState.config.lengthInBeats);
        }

        editorState.cameraController.startFrame(timeInBeats, isEndKeyframe);

        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) saveDialog.open();
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        mouseState.newFrame();
        keyboardState.newFrame();

        if (showDemoWindow) ImGui::ShowDemoWindow(&showDemoWindow);
        if (showSettingsWindow) renderSettingsWindow();
        debugWindow.render();

        editorState.cameraController.updateCamera(timeDeltaMs, keyboardState, mouseState);

        bool shouldRerender = isPlaying;

        shouldRerender |= handleKeyboard(keyboardState, timeInBeats);

        ImGui::SetNextWindowPos(ImVec2(0, windowRenderer.viewportHeight), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(windowRenderer.timelineWidth, windowRenderer.timelineHeight), ImGuiCond_Always);

        ImGui::Begin("Timeline", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

        shouldRerender |= renderToolbar(timeInBeats, info);

        if (renderTimelines(timeInBeats, timelineViewStart, timelineViewEnd, isEndKeyframe, editorState.config.uniformList,
                editorState.cameraController, editorState.currentGroup, editorState.config.lengthInBeats,
                isAbLooping ? &loopStartTime : nullptr, isAbLooping ? &loopEndTime : nullptr))
        {
            shouldRerender = true;
            frames = -1;
            isPlaying = false; // Pause when scrubbing
        }

        ImGui::SetItemTooltip("Space");

        ImGui::End();

        // Sidebar
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(windowRenderer.sidebarWidth, windowRenderer.sidebarHeight), ImGuiCond_Always);

        ImGui::Begin("Editor", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar |
                ImGuiWindowFlags_NoTitleBar);

        if (renderMenuBar()) shouldRerender = true;

        bool shouldReloadFragmentShader = false;
        if (renderAndUpdateUniforms(timeInBeats, isEndKeyframe, isPlaying, shouldReloadFragmentShader, editorState, actionHistory,
                windowRenderer.sidebarHeight)) // The function will pause if necessary
            shouldRerender = true;

        if (shouldReloadFragmentShader) windowRenderer.reload(editorState.config.uniformList, editorState.currentGroup);

        editorState.cameraController.displayImGuiWindow();
        shouldRerender |= editorState.cameraController.didCameraMove();

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
        bool didReload = windowRenderer.reloadFromFile(editorState.config.uniformList, editorState.currentGroup);
        shouldRerender |= didReload;

        EditorMusic::Update(isPlaying && shouldPlayMusic, timeInBeats, editorState.config.bpm);

        if (prevShouldRerender && !shouldRerender)
        {
            introLoop(timeInBeats);
            SwapBuffers(info->hDC);
            introLoop(timeInBeats);
        }

        if (shouldRerender)
        {
            introLoop(timeInBeats);
            frames++;
        }

        // If isPlaying changed, propagate the change to the time variables
        playStartSystemTime = isPlaying ? currentSystemTime : -1;
        playStartTime = isPlaying ? timeInBeats : -1;

        prevShouldRerender = shouldRerender;

        if (saveDialog.render(done)) saveConfigToFile(configFileName);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SwapBuffers(info->hDC);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    EditorMusic::Shutdown();
    sndPlaySound(0, 0);
    window_end(info);

    return 0;
}
